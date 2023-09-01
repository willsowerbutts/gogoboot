/*
   Simple(ish) NE2000 driver
   Based on;
     sources from the Linux kernel (pcnet_cs.c, 8390.h, ne.c) and
     eCOS(if_dp83902a.c, if_dp83902a.h) and
     Port to U-Boot by Christian Pellegrin <chri@ascensit.com>

   Portions (c) 2023 William R Sowerbutts <will@sowerbutts.com>

*/

#include <stdlib.h>
#include <types.h>
#include <ne2000.h>
#include <q40/hw.h>
#include <net.h>
#include <cli.h>

/* forward definition of function used for the uboot interface */
static void push_packet_ready(int len);

#undef  DEBUG                   /* extra-chatty mode */

#ifdef TARGET_Q40
/* 16-bit bus targets: Q40 */
#include <q40/isa.h>
#define NE2000_16BIT_PIO        /* use 16-bit PIO data transfer */
static uint16_t const portlist[] = { 0x300, 0x280, 0x320, 0x340, 0x360, 0x380, 0 };
static inline void    write_port_byte_pause(uint16_t port, uint8_t val) { isa_write_byte_pause(port, val); }
static inline void    write_port_byte(uint16_t port, uint8_t val)       { isa_write_byte(port, val); }
static inline void    write_port_word(uint16_t port, uint16_t val)      { isa_write_word(port, val); }
static inline uint8_t  read_port_byte(uint16_t port)             { return isa_read_byte(port); }
static inline uint16_t read_port_word(uint16_t port)             { return isa_read_word(port); }
static inline void     io_slow_down(void)                               { isa_slow_down(); }
#else 
/* 8-bit bus targets: KISS-68030 */
#include <ecb/ecb.h>
#undef NE2000_16BIT_PIO         /* use 8-bit PIO data transfer */
static uint16_t const portlist[] = { 0 };
static inline void    write_port_byte_pause(uint16_t port, uint8_t val) { ecb_write_byte_pause(port, val); }
static inline void    write_port_byte(uint16_t port, uint8_t val)       { ecb_write_byte(port, val); }
static inline uint8_t  read_port_byte(uint16_t port)             { return ecb_read_byte(port); }
static inline void     io_slow_down(void)                               { ecb_slow_down(); }
#endif

#ifdef DEBUG
#define DEBUG_FUNCTION() do { printf("%s\n", __FUNCTION__); } while (0)
#define DEBUG_LINE() do { printf("%d\n", __LINE__); } while (0)
#else
#define DEBUG_FUNCTION() do {} while(0)
#define DEBUG_LINE() do {} while(0)
#endif

#include <ne2000.h>

#ifdef DEBUG
#define PRINTK(args...) printf(args)
#else
#define PRINTK(args...)
#endif

static dp83902a_priv_data_t nic;                /* just one instance of the card supported */

#ifdef DEBUG
static void ne2000_dump_regs(void)
{
    uint8_t start, stop, current, boundary;
    write_port_byte_pause(nic.base + DP_CR, DP_CR_PAGE2 | DP_CR_NODMA | DP_CR_START);
    start = read_port_byte(nic.base + DP_P2_PSTART);
    stop = read_port_byte(nic.base + DP_P2_PSTOP);
    write_port_byte_pause(nic.base + DP_CR, DP_CR_PAGE1 | DP_CR_NODMA | DP_CR_START);
    current = read_port_byte(nic.base + DP_P1_CURP);
    write_port_byte_pause(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
    boundary = read_port_byte(nic.base + DP_BNDRY);

    printf("ne2000: start=0x%02x, stop=0x%02x, current/w=0x%02x, boundary/r=0x%02x, rxnext=0x%02x\n",
            start, stop, current, boundary, nic.rx_next);
}
#endif

static void dp83902a_stop(void)
{
    DEBUG_FUNCTION();

    write_port_byte_pause(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_STOP);  /* Brutal */
    write_port_byte_pause(nic.base + DP_ISR, 0xFF);             /* Clear any pending interrupts */
    write_port_byte_pause(nic.base + DP_IMR, 0x00);             /* Disable all interrupts */

    nic.running = false;
}

/*
   This function is called to "start up" the interface.  It may be called
   multiple times, even when the hardware is already running.  It will be
   called whenever something "hardware oriented" changes and should leave
   the hardware ready to send/receive packets.
   */
static void dp83902a_start(macaddr_t enaddr)
{
    int i;

    DEBUG_FUNCTION();

    write_port_byte_pause(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_STOP); /* Brutal */
#ifdef NE2000_16BIT_PIO
    // DP_DCR_BOS does not seem to affect the actual byte order the card uses ... ?
    write_port_byte_pause(nic.base + DP_DCR, DP_DCR_LS | DP_DCR_FIFO_4 | DP_DCR_WTS);
#else
    write_port_byte_pause(nic.base + DP_DCR, DP_DCR_LS | DP_DCR_FIFO_4);
#endif
    write_port_byte_pause(nic.base + DP_RBCH, 0);               /* Remote byte count */
    write_port_byte_pause(nic.base + DP_RBCL, 0);
    write_port_byte_pause(nic.base + DP_RCR, DP_RCR_MON);       /* Accept no packets */
    write_port_byte_pause(nic.base + DP_TCR, DP_TCR_LOCAL);     /* Transmitter [virtually] off */
    write_port_byte_pause(nic.base + DP_TPSR, nic.tx_buf1);     /* Transmitter start page */
    nic.tx1 = nic.tx2 = 0;
    nic.tx_next = nic.tx_buf1;
    nic.tx_started = false;

    write_port_byte_pause(nic.base + DP_PSTART, nic.rx_buf_start); /* Receive ring start page */
    write_port_byte_pause(nic.base + DP_PSTOP, nic.rx_buf_end);    /* Receive ring end page */
    write_port_byte_pause(nic.base + DP_BNDRY, nic.rx_buf_start);  /* Receive ring boundary (= host read pointer) */
    write_port_byte_pause(nic.base + DP_ISR, 0xFF);             /* Clear any pending interrupts */
    write_port_byte_pause(nic.base + DP_IMR, DP_IMR_All);       /* Enable all interrupts */
    write_port_byte_pause(nic.base + DP_CR, DP_CR_NODMA | DP_CR_PAGE1 | DP_CR_STOP);  /* Select page 1 */
    write_port_byte_pause(nic.base + DP_P1_CURP, nic.rx_buf_start+1); /* Current page (= receiver write pointer) */
    nic.rx_next = nic.rx_buf_start+1;
    for (i = 0;  i < 6;  i++) {
        write_port_byte_pause(nic.base + DP_P1_PAR0+i, enaddr[i]);
    }
    /* Enable and start device */
    write_port_byte_pause(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
    write_port_byte_pause(nic.base + DP_TCR, DP_TCR_NORMAL); /* Normal transmit operations */
    write_port_byte_pause(nic.base + DP_RCR, DP_RCR_AB);  /* Accept broadcast, no errors, no multicast */
    nic.running = true;

#ifdef DEBUG
    printf("ne2000: init complete\n");
    ne2000_dump_regs();
#endif
}

/*
   This routine is called to start the transmitter.  It is split out from the
   data handling routine so it may be called either when data becomes first
   available or when an Tx interrupt occurs
   */
static void dp83902a_start_xmit(int start_page, int len)
{
    DEBUG_FUNCTION();

#ifdef DEBUG
    printf("Tx pkt %d len %d\n", start_page, len);
    if (nic.tx_started)
        printf("TX already started?!?\n");
#endif

    write_port_byte_pause(nic.base + DP_ISR, (DP_ISR_TxP | DP_ISR_TxE));
    write_port_byte_pause(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
    write_port_byte_pause(nic.base + DP_TBCL, len & 0xFF);
    write_port_byte_pause(nic.base + DP_TBCH, len >> 8);
    write_port_byte_pause(nic.base + DP_TPSR, start_page);
    write_port_byte_pause(nic.base + DP_CR, DP_CR_NODMA | DP_CR_TXPKT | DP_CR_START);

    nic.tx_started = true;
}

/*
   This routine is called to send data to the hardware.  It is known a-priori
   that there is free buffer space (nic.tx_next).
   */
static void dp83902a_send(void *data, int total_len)
{
    int len, start_page, pkt_len, i, isr;

    DEBUG_FUNCTION();

    len = pkt_len = total_len;
    if (pkt_len < IEEE_8023_MIN_FRAME)
        pkt_len = IEEE_8023_MIN_FRAME;

    start_page = nic.tx_next;
    if (nic.tx_next == nic.tx_buf1) {
        PRINTK("tx1 ");
        nic.tx1 = start_page;
        nic.tx1_len = pkt_len;
        nic.tx_next = nic.tx_buf2;
    } else {
        PRINTK("tx2 ");
        nic.tx2 = start_page;
        nic.tx2_len = pkt_len;
        nic.tx_next = nic.tx_buf1;
    }

    PRINTK("total_len=%d pkt_len=%d ", total_len, pkt_len);

    write_port_byte_pause(nic.base + DP_ISR, DP_ISR_RDC);  /* Clear end of DMA */

    /* Dummy read. The manual says something slightly different, */
    /* but the code is extended a bit to do what Hitachi's monitor */
    /* does (i.e., also read data). */
    uint16_t __attribute__((unused)) tmp;
    write_port_byte_pause(nic.base + DP_RSAL, 0x100-1);
    write_port_byte_pause(nic.base + DP_RSAH, (start_page-1) & 0xff);
#ifdef NE2000_16BIT_PIO
    write_port_byte_pause(nic.base + DP_RBCL, 2);
#else
    write_port_byte_pause(nic.base + DP_RBCL, 1);
#endif
    write_port_byte_pause(nic.base + DP_RBCH, 0);
    write_port_byte_pause(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_RDMA | DP_CR_START);
#ifdef NE2000_16BIT_PIO
    tmp = read_port_word(nic.data);
#else
    tmp = read_port_byte(nic.data);
#endif

#ifdef NE2000_16BIT_PIO
    /* round up pkt_len for word writes */
    if(pkt_len & 1)
        pkt_len++;
#endif

    /* Send data to device buffer(s) */
    write_port_byte_pause(nic.base + DP_RSAL, 0);
    write_port_byte_pause(nic.base + DP_RSAH, start_page);
    write_port_byte_pause(nic.base + DP_RBCL, pkt_len & 0xFF);
    write_port_byte_pause(nic.base + DP_RBCH, pkt_len >> 8);
    write_port_byte_pause(nic.base + DP_CR, DP_CR_WDMA | DP_CR_START);

    /* Put data into buffer */
#ifdef NE2000_16BIT_PIO
    uint16_t *txptr = (uint16_t*)data;
    if(len & 1)
        len++;
    len = len >> 1;
#else
    uint8_t *txptr = (uint8_t*)data;
#endif
    for(i=0; i<len; i++){
#ifdef NE2000_16BIT_PIO
        write_port_word(nic.data, __builtin_bswap16(*(txptr++)));
#else
        write_port_byte(nic.data, *(txptr++));
#endif
    }

    /* pad with zeroes if required */
    if (total_len < pkt_len) {
#ifdef NE2000_16BIT_PIO
        len = len << 1;
#endif
        len = pkt_len - len;
#ifdef DEBUG
        printf("  + %d bytes of padding\n", pkt_len - total_len);
#endif
#ifdef NE2000_16BIT_PIO
        len = len >> 1;
#else
#endif
        /* Padding to 802.3 length was required */
        for(i=0; i<len; i++){
#ifdef NE2000_16BIT_PIO
            write_port_word(nic.data, 0);
#else
            write_port_byte(nic.data, 0);
#endif
        }
    }

    /* Wait for DMA to complete */
    do {
        isr = read_port_byte(nic.base + DP_ISR);
    } while ((isr & DP_ISR_RDC) == 0);

    /* Then disable DMA */
    write_port_byte_pause(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);

    /* Start transmit if not already going */
    if (!nic.tx_started) {
        if (start_page == nic.tx1) {
            nic.tx_int = 1;  /* Expecting interrupt from BUF1 */
        } else {
            nic.tx_int = 2;  /* Expecting interrupt from BUF2 */
        }
        dp83902a_start_xmit(start_page, pkt_len);
    }
}

/*
   This function is called when a packet has been received.  It's job is
   to prepare to unload the packet from the hardware.  Once the length of
   the packet is known, the upper layer of the driver can be told.  When
   the upper layer is ready to unload the packet, the internal function
   'dp83902a_recv' will be called to actually fetch it from the hardware.
   */
static void dp83902a_RxEvent(void)
{
    //uint8_t __attribute__((unused)) rsr;
    uint8_t rcv_hdr[4];
    int i, len, cur; //, pkt, cur;

    DEBUG_FUNCTION();

    //rsr = read_port_byte(nic.base + DP_RSR);
    while (true) {
#ifdef DEBUG
        printf("ne2000: attempt receive\n");
        ne2000_dump_regs();
#endif
        /* Read incoming packet header */
        write_port_byte_pause(nic.base + DP_CR, DP_CR_PAGE1 | DP_CR_NODMA | DP_CR_START);
        cur = read_port_byte(nic.base + DP_P1_CURP);
        write_port_byte_pause(nic.base + DP_P1_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);

        if(nic.rx_next == cur) // done reading packets?
            break;

        write_port_byte_pause(nic.base + DP_RBCL, sizeof(rcv_hdr));
        write_port_byte_pause(nic.base + DP_RBCH, 0);
        write_port_byte_pause(nic.base + DP_RSAL, 0);
        write_port_byte_pause(nic.base + DP_RSAH, nic.rx_next);
        write_port_byte_pause(nic.base + DP_ISR, DP_ISR_RDC); /* Clear end of DMA */
        io_slow_down();
        write_port_byte_pause(nic.base + DP_CR, DP_CR_RDMA | DP_CR_START);
        io_slow_down();

#ifdef NE2000_16BIT_PIO
        for (i = 0;  i < sizeof(rcv_hdr)/2; i++) {
            ((uint16_t*)rcv_hdr)[i] = __builtin_bswap16(read_port_word(nic.data));
        }
#else
        for (i = 0;  i < sizeof(rcv_hdr);) {
            rcv_hdr[i++] = read_port_byte(nic.data);
        }
#endif

#ifdef DEBUG
        printf("ne2000: rx header %02x %02x %02x %02x\n",
                rcv_hdr[0], rcv_hdr[1], rcv_hdr[2], rcv_hdr[3]);
#endif

        len = ((rcv_hdr[3] << 8) | rcv_hdr[2]) - sizeof(rcv_hdr);
        if (len>=PACKET_MAXLEN) {
            printf("ne2000: rx too big\n");
        }else{
            push_packet_ready(len);
        }

        nic.rx_next = rcv_hdr[1];
        if(nic.rx_next - 1 < nic.rx_buf_start)
            write_port_byte_pause(nic.base + DP_BNDRY, nic.rx_buf_end - 1);
        else
            write_port_byte_pause(nic.base + DP_BNDRY, nic.rx_next - 1);
#ifdef DEBUG
        printf("ne2000: update pointers\n");
        ne2000_dump_regs();
#endif
    }
}

/*
   This function is called as a result of the "eth_drv_recv()" call above.
   It's job is to actually fetch data for a packet from the hardware once
   memory buffers have been allocated for the packet.
   */
static void dp83902a_recv(uint8_t *data, int len)
{
    /* Read incoming packet data */
    write_port_byte_pause(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
    write_port_byte_pause(nic.base + DP_RBCL, len & 0xFF);
    write_port_byte_pause(nic.base + DP_RBCH, len >> 8);
    write_port_byte_pause(nic.base + DP_RSAL, 4);         /* Past header */
    write_port_byte_pause(nic.base + DP_RSAH, nic.rx_next);
    write_port_byte_pause(nic.base + DP_ISR, DP_ISR_RDC); /* Clear end of DMA */
    io_slow_down();
    write_port_byte_pause(nic.base + DP_CR, DP_CR_RDMA | DP_CR_START);
    io_slow_down();

#ifdef NE2000_16BIT_PIO
    uint16_t *dptr = (uint16_t*)data;
    len = (len+1) >> 1;
#else
    uint8_t *dptr = data;
#endif

    for(int i=0; i<len; i++){
#ifdef NE2000_16BIT_PIO
        *(dptr++) = __builtin_bswap16(read_port_word(nic.data));
#else
        *(dptr++) = read_port_byte(nic.data);
#endif
    }
}

static void dp83902a_TxEvent(void)
{
    uint8_t __attribute__((unused)) tsr;

    DEBUG_FUNCTION();

    tsr = read_port_byte(nic.base + DP_TSR);
    if (nic.tx_int == 1) {
        PRINTK("f1 ");
        nic.tx1 = 0;
    } else {
        PRINTK("f2 ");
        nic.tx2 = 0;
    }

    /* Start next packet if one is ready */
    nic.tx_started = false;

    if (nic.tx1) {
        dp83902a_start_xmit(nic.tx1, nic.tx1_len);
        nic.tx_int = 1;
    } else if (nic.tx2) {
        dp83902a_start_xmit(nic.tx2, nic.tx2_len);
        nic.tx_int = 2;
    } else {
        nic.tx_int = 0;
    }
}

/* Read the tally counters to clear them.  Called in response to a CNT */
/* interrupt. */
static void dp83902a_ClearCounters(void)
{
    uint8_t __attribute__((unused)) cnt1, cnt2, cnt3;

    cnt1 = read_port_byte(nic.base + DP_FER);
    cnt2 = read_port_byte(nic.base + DP_CER);
    cnt3 = read_port_byte(nic.base + DP_MISSED);
    write_port_byte_pause(nic.base + DP_ISR, DP_ISR_CNT);
}

/* Deal with an overflow condition.  This code follows the procedure set */
/* out in section 7.0 of the datasheet. */
static void dp83902a_Overflow(void)
{
    uint8_t isr;

    printf("ne2000: overflow\n");

    /* Issue a stop command and wait 1.6ms for it to complete. */
    write_port_byte_pause(nic.base + DP_CR, DP_CR_STOP | DP_CR_NODMA);

    /* Clear the remote byte counter registers. */
    write_port_byte_pause(nic.base + DP_RBCL, 0);
    write_port_byte_pause(nic.base + DP_RBCH, 0);

    /* Enter loopback mode while we clear the buffer. */
    write_port_byte_pause(nic.base + DP_TCR, DP_TCR_LOCAL);
    write_port_byte_pause(nic.base + DP_CR, DP_CR_START | DP_CR_NODMA);

    /* Read in as many packets as we can and acknowledge any and receive */
    /* interrupts.  Since the buffer has overflowed, a receive event of */
    /* some kind will have occured. */
    dp83902a_RxEvent();
    write_port_byte_pause(nic.base + DP_ISR, DP_ISR_RxP|DP_ISR_RxE);

    /* Clear the overflow condition and leave loopback mode. */
    write_port_byte_pause(nic.base + DP_ISR, DP_ISR_OFLW);
    write_port_byte_pause(nic.base + DP_TCR, DP_TCR_NORMAL);

    /* If a transmit command was issued, but no transmit event has occured, */
    /* restart it here. */
    isr = read_port_byte(nic.base + DP_ISR);
    if (nic.tx_started && !(isr & (DP_ISR_TxP|DP_ISR_TxE))) {
        write_port_byte_pause(nic.base + DP_CR, DP_CR_NODMA | DP_CR_TXPKT | DP_CR_START);
    }
}

static void dp83902a_poll(void)
{
    uint8_t isr;

    write_port_byte_pause(nic.base + DP_CR, DP_CR_NODMA | DP_CR_PAGE0 | DP_CR_START);
    do{
        isr = read_port_byte(nic.base + DP_ISR);
        /* The CNT interrupt triggers when the MSB of one of the error */
        /* counters is set.  We don't much care about these counters, but */
        /* we should read their values to reset them. */
        if (isr & DP_ISR_CNT) {
            dp83902a_ClearCounters();
        }
        /* Check for overflow.  It's a special case, since there's a */
        /* particular procedure that must be followed to get back into */
        /* a running state. */
        if (isr & DP_ISR_OFLW) {
            dp83902a_Overflow();
        } else {
            /* Other kinds of interrupts can be acknowledged simply by */
            /* clearing the relevant bits of the ISR.  Do that now, then */
            /* handle the interrupts we care about. */
            write_port_byte_pause(nic.base + DP_ISR, isr);      /* Clear set bits */
            if (!nic.running)
                break;        /* Is this necessary? */
            /* Check for tx_started on TX event since these may happen */
            /* spuriously it seems. */
            if (isr & (DP_ISR_TxP|DP_ISR_TxE) && nic.tx_started) {
                dp83902a_TxEvent();
            }
            if (isr & (DP_ISR_RxP|DP_ISR_RxE)) {
                dp83902a_RxEvent();
            }
        }
    }while(isr);
}

static void pcnet_reset_8390(void)
{
    int i, r;

    PRINTK("nic base is 0x%x\n", (int)nic.base);

    write_port_byte_pause(nic.base + E8390_CMD, E8390_NODMA+E8390_PAGE0+E8390_STOP);
    write_port_byte_pause(nic.base + E8390_CMD, E8390_NODMA+E8390_PAGE1+E8390_STOP);
    write_port_byte_pause(nic.base + E8390_CMD, E8390_NODMA+E8390_PAGE0+E8390_STOP);
    write_port_byte_pause(nic.base + E8390_CMD, E8390_NODMA+E8390_PAGE0+E8390_STOP);

    // set (read) and clear (write) reset line
    write_port_byte_pause(nic.base + PCNET_RESET, read_port_byte(nic.base + PCNET_RESET));

    for (i = 0; i < 100; i++) {
        delay_ms(5);
        if ((r = (read_port_byte(nic.base + EN0_ISR) & ENISR_RESET)) != 0)
            break;
    }
    write_port_byte_pause(nic.base + EN0_ISR, 0xff); /* Ack all interrupts */
}

static const struct {
    uint8_t value, offset;
} get_prom_program_seq[] = {
    {E8390_NODMA+E8390_PAGE0+E8390_STOP, E8390_CMD}, /* Select page 0 */
    { DP_DCR_LS | DP_DCR_FIFO_4, DP_DCR}, /* Byte-wide transfers */
    {0x00,      EN0_RCNTLO},    /* Clear the count regs. */
    {0x00,      EN0_RCNTHI},
    {0x00,      EN0_IMR},       /* Mask completion irq. */
    {0xFF,      EN0_ISR},
    {E8390_RXOFF, EN0_RXCR},    /* 0x20  Set to monitor */
    {E8390_TXOFF, EN0_TXCR},    /* 0x02  and loopback mode. */
    {32,        EN0_RCNTLO},
    {0x00,      EN0_RCNTHI},
    {0x00,      EN0_RSARLO},    /* DMA starting at 0x0000. */
    {0x00,      EN0_RSARHI},
    {E8390_RREAD+E8390_START, E8390_CMD},
};

static bool get_prom(void)
{
    uint8_t prom[32];
    int i, j;
    bool all_zero = true, all_ones = true;

    PRINTK("trying to get MAC via prom reading\n");

    pcnet_reset_8390();

    delay_ms(10);

    for (i = 0; i < sizeof(get_prom_program_seq)/sizeof(get_prom_program_seq[0]); i++)
        write_port_byte_pause(nic.base + get_prom_program_seq[i].offset, get_prom_program_seq[i].value);

    PRINTK("PROM:");
    for (i = 0; i < 32; i++) {
        prom[i] = read_port_byte(nic.data);
        PRINTK(" %02x", prom[i]);
    }
    PRINTK("\n");

    /* used to be a match against a long-ish table here.
     * I ripped it out and replaced it with a simpler
     * check that the MAC address is not all 0 or 1 bits
     */
    for (j = 0; j < 6; j++){
        interface_macaddr[j] = prom[j<<1];
        if(interface_macaddr[j] != 0xff)
            all_ones = false;
        if(interface_macaddr[j] != 0x00)
            all_zero = false;
    }

    if(all_zero || all_ones)
        return false;

    return true;
}

static void push_packet_ready(int len)
{
    PRINTK("pushed len = %d\n", len);

    packet_t *packet = packet_alloc(len);
    if(!packet){
        printf("ne2000: no free rx buffer\n");
        return;
    }
    dp83902a_recv(packet->buffer, packet->buffer_length);
    net_eth_push(packet);
}

bool eth_init(void)
{
    for(int i=0; portlist[i]; i++){
        nic.base = portlist[i];
        nic.data = nic.base + DP_DATAPORT;

        if(!get_prom())
            continue;

        nic.tx_buf1 = 0x40; /* 2KB */
        nic.tx_buf2 = 0x48; /* 2KB */
        nic.rx_buf_start = 0x50; /* 12KB */
        nic.rx_buf_end = 0x80;

        printf("NE2000 at 0x%x, MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
                nic.base,
                interface_macaddr[0], interface_macaddr[1], interface_macaddr[2],
                interface_macaddr[3], interface_macaddr[4], interface_macaddr[5]);

        dp83902a_start(interface_macaddr);

        return true;
    }

    printf("No NE2000 card found\n");
    nic.base = nic.data = 0;

    return false;
}

int eth_rxbuffer_size(void)
{
    int r;
    if(!nic.base)
        return 0;
    r = (nic.rx_buf_end - nic.rx_buf_start) << 8; // 256 byte pages
    return r;
}

void eth_halt(void)
{
    if(nic.base)
        dp83902a_stop();
}

static bool eth_tx(uint8_t *packet, int length)
{
    if(!nic.base){
        return false;
    }
    if (length >= PACKET_MAXLEN) {
        printf("ne2000: tx too big\n");
        return false;
    }
    if(nic.tx1 && nic.tx2){
        return false;
    }else{
        dp83902a_send(packet, length);
        return true;
    }
}

bool eth_attempt_tx(packet_t *packet)
{
    return eth_tx(packet->buffer, packet->buffer_length);
}

void eth_pump(void)
{
    packet_t *packet;

    if(!nic.base)
        return;

    dp83902a_poll();

    while(!(nic.tx1 && nic.tx2)){
        packet = net_eth_pull();
        if(!packet)
            break;
        if(!eth_attempt_tx(packet))
            printf("ne2000: eth_tx failed\n");
        packet_free(packet);
    }
}
