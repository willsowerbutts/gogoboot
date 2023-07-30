/*
   Simple(ish) NE2000 driver 
   Based on;
     sources from the Linux kernel (pcnet_cs.c, 8390.h, ne.c) and
     eCOS(if_dp83902a.c, if_dp83902a.h) and
     Port to U-Boot by Christian Pellegrin <chri@ascensit.com>
*/

#include <stdlib.h>
#include <q40types.h>
#include "q40isa.h"
#include "q40hw.h"
#include "net.h"
#include "cli.h"
#include "8390.h"

/* forward definition of function used for the uboot interface */
static void push_packet_ready(int len);

/* timeout for tx/rx in s */
#define TIMEOUT_SEC 5
#define NUMPKTBUF   5

#define DEBUG 

#ifdef DEBUG
#define DEBUG_FUNCTION() do { printf("%s\n", __FUNCTION__); } while (0)
#define DEBUG_LINE() do { printf("%d\n", __LINE__); } while (0)
#else
#define DEBUG_FUNCTION() do {} while(0)
#define DEBUG_LINE() do {} while(0)
#endif

#include "ne2000.h"

#ifdef DEBUG
#define PRINTK(args...) printf(args)
#else
#define PRINTK(args...)
#endif

static dp83902a_priv_data_t nic;                /* just one instance of the card supported */
static uint8_t dev_addr[6];                     /* MAC address for card found */
static uint8_t pbuf[NUMPKTBUF][MAXPKTLEN];      /* buffered packet data */
static int plen[NUMPKTBUF];                     /* buffered packet lengths */
int pkt_buffered_first, pkt_buffered_count;     /* ring buffer: start + length */

static bool dp83902a_init(void)
{
    int i;

    DEBUG_FUNCTION();

    if (!nic.base) 
        return false;  /* No device found */

    DEBUG_LINE();

    /* Prepare ESA */
    isa_write_byte(nic.base + DP_CR, DP_CR_NODMA | DP_CR_PAGE1);  /* Select page 1 */

    /* Use the address from the serial EEPROM */
    for (i = 0; i < 6; i++)
        nic.esa[i] = isa_read_byte(nic.base + DP_P1_PAR0+i);
    isa_write_byte(nic.base + DP_CR, DP_CR_NODMA | DP_CR_PAGE0);  /* Select page 0 */

    printf("NE2000 - ESA: %02x:%02x:%02x:%02x:%02x:%02x\n",
            nic.esa[0], nic.esa[1], nic.esa[2], 
            nic.esa[3], nic.esa[4], nic.esa[5] );

    return true;
}

static void dp83902a_stop(void)
{
    DEBUG_FUNCTION();

    isa_write_byte(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_STOP);  /* Brutal */
    isa_write_byte(nic.base + DP_ISR, 0xFF);             /* Clear any pending interrupts */
    isa_write_byte(nic.base + DP_IMR, 0x00);             /* Disable all interrupts */

    nic.running = false;
}

/*
   This function is called to "start up" the interface.  It may be called
   multiple times, even when the hardware is already running.  It will be
   called whenever something "hardware oriented" changes and should leave
   the hardware ready to send/receive packets.
   */
static void dp83902a_start(uint8_t *enaddr)
{
    int i;

    DEBUG_FUNCTION();

    isa_write_byte(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_STOP); /* Brutal */
    isa_write_byte(nic.base + DP_DCR, DP_DCR_INIT);
    isa_write_byte(nic.base + DP_RBCH, 0);               /* Remote byte count */
    isa_write_byte(nic.base + DP_RBCL, 0);
    isa_write_byte(nic.base + DP_RCR, DP_RCR_MON);       /* Accept no packets */
    isa_write_byte(nic.base + DP_TCR, DP_TCR_LOCAL);     /* Transmitter [virtually] off */
    isa_write_byte(nic.base + DP_TPSR, nic.tx_buf1);     /* Transmitter start page */
    nic.tx1 = nic.tx2 = 0;
    nic.tx_next = nic.tx_buf1;
    nic.tx_started = false;
    isa_write_byte(nic.base + DP_PSTART, nic.rx_buf_start); /* Receive ring start page */
    isa_write_byte(nic.base + DP_BNDRY, nic.rx_buf_end-1); /* Receive ring boundary */
    isa_write_byte(nic.base + DP_PSTOP, nic.rx_buf_end); /* Receive ring end page */
    nic.rx_next = nic.rx_buf_start-1;
    isa_write_byte(nic.base + DP_ISR, 0xFF);             /* Clear any pending interrupts */
    isa_write_byte(nic.base + DP_IMR, DP_IMR_All);       /* Enable all interrupts */
    isa_write_byte(nic.base + DP_CR, DP_CR_NODMA | DP_CR_PAGE1 | DP_CR_STOP);  /* Select page 1 */
    isa_write_byte(nic.base + DP_P1_CURP, nic.rx_buf_start);   /* Current page - next free page for Rx */
    for (i = 0;  i < 6;  i++) {
        isa_write_byte(nic.base + DP_P1_PAR0+i, enaddr[i]);
    }
    /* Enable and start device */
    isa_write_byte(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
    isa_write_byte(nic.base + DP_TCR, DP_TCR_NORMAL); /* Normal transmit operations */
    isa_write_byte(nic.base + DP_RCR, DP_RCR_AB);  /* Accept broadcast, no errors, no multicast */
    nic.running = true;
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

    isa_write_byte(nic.base + DP_ISR, (DP_ISR_TxP | DP_ISR_TxE));
    isa_write_byte(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
    isa_write_byte(nic.base + DP_TBCL, len & 0xFF);
    isa_write_byte(nic.base + DP_TBCH, len >> 8);
    isa_write_byte(nic.base + DP_TPSR, start_page);
    isa_write_byte(nic.base + DP_CR, DP_CR_NODMA | DP_CR_TXPKT | DP_CR_START);

    nic.tx_started = true;
}

/*
   This routine is called to send data to the hardware.  It is known a-priori
   that there is free buffer space (nic.tx_next).
   */
static void dp83902a_send(void *data, int total_len)
{
    int len, start_page, pkt_len, i, isr;
#ifdef DEBUG
    int dx;
#endif

    DEBUG_FUNCTION();

    len = pkt_len = total_len;
    if (pkt_len < IEEE_8023_MIN_FRAME)
        pkt_len = IEEE_8023_MIN_FRAME;

    start_page = nic.tx_next;
    if (nic.tx_next == nic.tx_buf1) {
        nic.tx1 = start_page;
        nic.tx1_len = pkt_len;
        nic.tx_next = nic.tx_buf2;
    } else {
        nic.tx2 = start_page;
        nic.tx2_len = pkt_len;
        nic.tx_next = nic.tx_buf1;
    }

#ifdef DEBUG
    printf("TX prep page %d len %d\n", start_page, pkt_len);
#endif

    isa_write_byte(nic.base + DP_ISR, DP_ISR_RDC);  /* Clear end of DMA */

    /* Dummy read. The manual sez something slightly different, */
    /* but the code is extended a bit to do what Hitachi's monitor */
    /* does (i.e., also read data). */
    uint16_t __attribute__((unused)) tmp;
    isa_write_byte(nic.base + DP_RSAL, 0x100-1);
    isa_write_byte(nic.base + DP_RSAH, (start_page-1) & 0xff);
    isa_write_byte(nic.base + DP_RBCL, 1);
    isa_write_byte(nic.base + DP_RBCH, 0);
    isa_write_byte(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_RDMA | DP_CR_START);
    tmp = isa_read_byte(nic.data);

#ifdef CYGHWR_NS_DP83902A_PLF_BROKEN_TX_DMA
    /* Stall for a bit before continuing to work around random data */
    /* corruption problems on some platforms. */
    CYGACC_CALL_IF_DELAY_US(1);
#endif

    /* Send data to device buffer(s) */
    isa_write_byte(nic.base + DP_RSAL, 0);
    isa_write_byte(nic.base + DP_RSAH, start_page);
    isa_write_byte(nic.base + DP_RBCL, pkt_len & 0xFF);
    isa_write_byte(nic.base + DP_RBCH, pkt_len >> 8);
    isa_write_byte(nic.base + DP_CR, DP_CR_WDMA | DP_CR_START);

    /* Put data into buffer */
#ifdef DEBUG
    printf(" sg buf %08lx len %08x\n ", (unsigned long) data, len);
    dx = 0;
#endif
    uint8_t *txptr = (uint8_t*)data;
    while (len > 0) {
#ifdef DEBUG
        printf(" %02x", *txptr);
        if (0 == (++dx % 16)) 
            printf("\n ");
#endif
        isa_write_byte(nic.data, *(txptr++));
        len--;
    }
#ifdef DEBUG
    printf("\n");
#endif
    if (total_len < pkt_len) {
#ifdef DEBUG
        printf("  + %d bytes of padding\n", pkt_len - total_len);
#endif
        /* Padding to 802.3 length was required */
        for (i = total_len;  i < pkt_len;) {
            i++;
            isa_write_byte(nic.data, 0);
        }
    }

#ifdef CYGHWR_NS_DP83902A_PLF_BROKEN_TX_DMA
    /* After last data write, delay for a bit before accessing the */
    /* device again, or we may get random data corruption in the last */
    /* datum (on some platforms). */
    CYGACC_CALL_IF_DELAY_US(1);
#endif

    /* Wait for DMA to complete */
    do {
        isr = isa_read_byte(nic.base + DP_ISR);
    } while ((isr & DP_ISR_RDC) == 0);
    /* Then disable DMA */
    isa_write_byte(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);

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
    uint8_t __attribute__((unused)) rsr;
    uint8_t rcv_hdr[4];
    int i, len, pkt, cur;

    DEBUG_FUNCTION();

    rsr = isa_read_byte(nic.base + DP_RSR);
    while (true) {
        /* Read incoming packet header */
        isa_write_byte(nic.base + DP_CR, DP_CR_PAGE1 | DP_CR_NODMA | DP_CR_START);
        cur = isa_read_byte(nic.base + DP_P1_CURP);
        isa_write_byte(nic.base + DP_P1_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
        pkt = isa_read_byte(nic.base + DP_BNDRY);

        pkt += 1;
        if (pkt == nic.rx_buf_end)
            pkt = nic.rx_buf_start;

        if (pkt == cur) {
            break;
        }
        isa_write_byte(nic.base + DP_RBCL, sizeof(rcv_hdr));
        isa_write_byte(nic.base + DP_RBCH, 0);
        isa_write_byte(nic.base + DP_RSAL, 0);
        isa_write_byte(nic.base + DP_RSAH, pkt);
        if (nic.rx_next == pkt) {
            if (cur == nic.rx_buf_start)
                isa_write_byte(nic.base + DP_BNDRY, nic.rx_buf_end-1);
            else
                isa_write_byte(nic.base + DP_BNDRY, cur-1); /* Update pointer */
            return;
        }
        nic.rx_next = pkt;
        isa_write_byte(nic.base + DP_ISR, DP_ISR_RDC); /* Clear end of DMA */
        isa_write_byte(nic.base + DP_CR, DP_CR_RDMA | DP_CR_START);
#ifdef CYGHWR_NS_DP83902A_PLF_BROKEN_RX_DMA
        CYGACC_CALL_IF_DELAY_US(10);
#endif

        for (i = 0;  i < sizeof(rcv_hdr);) {
            rcv_hdr[i++] = isa_read_byte(nic.data);
        }

#ifdef DEBUG
        printf("rx hdr %02x %02x %02x %02x\n",
                rcv_hdr[0], rcv_hdr[1], rcv_hdr[2], rcv_hdr[3]);
#endif
        len = ((rcv_hdr[3] << 8) | rcv_hdr[2]) - sizeof(rcv_hdr);
        push_packet_ready(len);
        if (rcv_hdr[1] == nic.rx_buf_start)
            isa_write_byte(nic.base + DP_BNDRY, nic.rx_buf_end-1);
        else
            isa_write_byte(nic.base + DP_BNDRY, rcv_hdr[1]-1); /* Update pointer */
    }
}

/*
   This function is called as a result of the "eth_drv_recv()" call above.
   It's job is to actually fetch data for a packet from the hardware once
   memory buffers have been allocated for the packet.  Note that the buffers
   may come in pieces, using a scatter-gather list.  This allows for more
   efficient processing in the upper layers of the stack.
   */
static void dp83902a_recv(uint8_t *data, int len)
{
    int mlen;
    uint8_t saved_char = 0;
    bool saved;
#ifdef DEBUG
    int dx;
#endif

    DEBUG_FUNCTION();

#ifdef DEBUG 
    printf("Rx packet %d length %d\n", nic.rx_next, len);
#endif

    /* Read incoming packet data */
    isa_write_byte(nic.base + DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
    isa_write_byte(nic.base + DP_RBCL, len & 0xFF);
    isa_write_byte(nic.base + DP_RBCH, len >> 8);
    isa_write_byte(nic.base + DP_RSAL, 4);               /* Past header */
    isa_write_byte(nic.base + DP_RSAH, nic.rx_next);
    isa_write_byte(nic.base + DP_ISR, DP_ISR_RDC); /* Clear end of DMA */
    isa_write_byte(nic.base + DP_CR, DP_CR_RDMA | DP_CR_START);
#ifdef CYGHWR_NS_DP83902A_PLF_BROKEN_RX_DMA
    CYGACC_CALL_IF_DELAY_US(10);
#endif

    saved = false;
    if (data) {
        mlen = len;
#ifdef DEBUG 
        printf(" sg buf %08lx len %08x \n", (unsigned long) data, mlen);
        dx = 0;
#endif
        while (0 < mlen) {
            /* Saved byte from previous loop? */
            if (saved) {
                *data++ = saved_char;
                mlen--;
                saved = false;
                continue;
            }

            {
                uint8_t tmp;
                tmp = isa_read_byte(nic.data);
#ifdef DEBUG 
                printf(" %02x", tmp);
                if (0 == (++dx % 16)) printf("\n ");
#endif
                *data++ = tmp;;
                mlen--;
            }
        }
#ifdef DEBUG 
        printf("\n");
#endif
    }
}

static void dp83902a_TxEvent(void)
{
    uint8_t __attribute__((unused)) tsr;

    DEBUG_FUNCTION();

    tsr = isa_read_byte(nic.base + DP_TSR);
    if (nic.tx_int == 1) {
        nic.tx1 = 0;
    } else {
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
    /* Could tell higher level we sent this packet ... but it doesn't care */
}

/* Read the tally counters to clear them.  Called in response to a CNT */
/* interrupt. */
static void dp83902a_ClearCounters(void)
{
    uint8_t __attribute__((unused)) cnt1, cnt2, cnt3;

    cnt1 = isa_read_byte(nic.base + DP_FER);
    cnt2 = isa_read_byte(nic.base + DP_CER);
    cnt3 = isa_read_byte(nic.base + DP_MISSED);
    isa_write_byte(nic.base + DP_ISR, DP_ISR_CNT);
}

/* Deal with an overflow condition.  This code follows the procedure set */
/* out in section 7.0 of the datasheet. */
static void dp83902a_Overflow(void)
{
    uint8_t isr;

    /* Issue a stop command and wait 1.6ms for it to complete. */
    isa_write_byte(nic.base + DP_CR, DP_CR_STOP | DP_CR_NODMA);
    CYGACC_CALL_IF_DELAY_US(1600);

    /* Clear the remote byte counter registers. */
    isa_write_byte(nic.base + DP_RBCL, 0);
    isa_write_byte(nic.base + DP_RBCH, 0);

    /* Enter loopback mode while we clear the buffer. */
    isa_write_byte(nic.base + DP_TCR, DP_TCR_LOCAL);
    isa_write_byte(nic.base + DP_CR, DP_CR_START | DP_CR_NODMA);

    /* Read in as many packets as we can and acknowledge any and receive */
    /* interrupts.  Since the buffer has overflowed, a receive event of */
    /* some kind will have occured. */
    dp83902a_RxEvent();
    isa_write_byte(nic.base + DP_ISR, DP_ISR_RxP|DP_ISR_RxE);

    /* Clear the overflow condition and leave loopback mode. */
    isa_write_byte(nic.base + DP_ISR, DP_ISR_OFLW);
    isa_write_byte(nic.base + DP_TCR, DP_TCR_NORMAL);

    /* If a transmit command was issued, but no transmit event has occured, */
    /* restart it here. */
    isr = isa_read_byte(nic.base + DP_ISR);
    if (nic.tx_started && !(isr & (DP_ISR_TxP|DP_ISR_TxE))) {
        isa_write_byte(nic.base + DP_CR, DP_CR_NODMA | DP_CR_TXPKT | DP_CR_START);
    }
}

static void dp83902a_poll(void)
{
    uint8_t isr;

    isa_write_byte(nic.base + DP_CR, DP_CR_NODMA | DP_CR_PAGE0 | DP_CR_START);
    do{
        isr = isa_read_byte(nic.base + DP_ISR);
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
            isa_write_byte(nic.base + DP_ISR, isr);      /* Clear set bits */
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

    isa_write_byte(nic.base + E8390_CMD, E8390_NODMA+E8390_PAGE0+E8390_STOP);
    isa_write_byte(nic.base + E8390_CMD, E8390_NODMA+E8390_PAGE1+E8390_STOP);
    isa_write_byte(nic.base + E8390_CMD, E8390_NODMA+E8390_PAGE0+E8390_STOP);
    isa_write_byte(nic.base + E8390_CMD, E8390_NODMA+E8390_PAGE0+E8390_STOP);

    // set (read) and clear (write) reset line
    isa_write_byte(nic.base + PCNET_RESET, isa_read_byte(nic.base + PCNET_RESET));

    for (i = 0; i < 100; i++) {
        if ((r = (isa_read_byte(nic.base + EN0_ISR) & ENISR_RESET)) != 0)
            break;
        delay_ms(5);
    }
    isa_write_byte(nic.base + EN0_ISR, 0xff); /* Ack all interrupts */
}

static const struct {
    uint8_t value, offset;
} get_prom_program_seq[] = {
    {E8390_NODMA+E8390_PAGE0+E8390_STOP, E8390_CMD}, /* Select page 0*/
    {0x48,      EN0_DCFG},      /* Set byte-wide (0x48) access. */
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
        isa_write_byte(nic.base + get_prom_program_seq[i].offset, get_prom_program_seq[i].value);

    PRINTK("PROM:");
    for (i = 0; i < 32; i++) {
        prom[i] = isa_read_byte(nic.data);
        PRINTK(" %02x", prom[i]);
    }
    PRINTK("\n");

    /* used to be a match against a long-ish table here.
     * I ripped it out and replaced it with a simpler
     * check that the MAC address is not all 0 or 1 bits
     */
    for (j = 0; j < 6; j++){
        dev_addr[j] = prom[j<<1];
        if(dev_addr[j] != 0xff)
            all_ones = false;
        if(dev_addr[j] != 0x00)
            all_zero = false;
    }

    PRINTK("MAC address is %02x:%02x:%02x:%02x:%02x:%02x\n",
            dev_addr[0],dev_addr[1],dev_addr[2],dev_addr[3],dev_addr[4],dev_addr[5]);

    if(all_zero || all_ones)
        return false;

    return true;
}

static void push_packet_ready(int len) 
{
    PRINTK("pushed len = %d\n", len);
    if (len>=MAXPKTLEN) {
        printf("NE2000: packet too big\n");
        return;
    }
    if(pkt_buffered_count == NUMPKTBUF){
        printf("NE2000: losing packets in rx\n");
        return;
    }
    /* push packet into tail of ring buffer */
    int rxpkt = (pkt_buffered_first + pkt_buffered_count) % NUMPKTBUF;
    plen[rxpkt] = len;
    dp83902a_recv(pbuf[rxpkt], len);
    pkt_buffered_count++;
}

bool eth_init(void) 
{
    nic.base = CONFIG_DRIVER_NE2000_BASE;

    if(!get_prom())
        return false;

    nic.data = nic.base + DP_DATAPORT;
    nic.tx_buf1 = 0x40;
    nic.tx_buf2 = 0x48;
    nic.rx_buf_start = 0x50;
    nic.rx_buf_end = 0x80;

    pkt_buffered_first = 0;
    pkt_buffered_count = 0;

    if (!dp83902a_init())
        return false;

    dp83902a_start(dev_addr);

    return true;
}

void eth_halt(void) 
{
    dp83902a_stop();
}

uint8_t *eth_rx(int *length)
{
    if(pkt_buffered_count == 0)
        dp83902a_poll(); /* pump the card */
    if(pkt_buffered_count){
        /* pop a packet from the head of the ring buffer */
        uint8_t *r = pbuf[pkt_buffered_first];
        *length = plen[pkt_buffered_first];
        pkt_buffered_count--;
        pkt_buffered_first = (pkt_buffered_first + 1) % NUMPKTBUF;
        return r;
    }
    return NULL;
}

bool eth_tx(uint8_t *packet, int length)
{
    dp83902a_poll();

    PRINTK("### eth_tx\n");
    PRINTK("TODO: CHECK TXBUF FREE BEFORE PROCEEDING!?\n");

    dp83902a_send(packet, length);

    return true;
}
