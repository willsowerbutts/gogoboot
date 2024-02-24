/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <uart.h>
#include <stdlib.h>

#if defined(TARGET_Q40) /* ISA based targets */
    #include <q40/isa.h>
    #define UART_ADDRESS    0x3f8
    #define UARTCLOCK       1843200
    static inline uint8_t uart_inb(uint16_t port) { return isa_read_byte(port); }
    static inline void uart_outb(uint16_t port, uint8_t val) { isa_write_byte(port, val); }
#elif defined(TARGET_KISS) || defined(TARGET_MINI) /* ECB based targets */
    #include <ecb/ecb.h>
    #define UART_ADDRESS     MFPIC_UART
    #define UARTCLOCK        MFPIC_UART_CLK
    static inline uint8_t uart_inb(uint16_t port) { return ecb_read_byte(port); }
    static inline void uart_outb(uint16_t port, uint8_t val) { ecb_write_byte(port, val); }
#else
    #pragma error update uart.c for your target
#endif

#ifndef BAUD_RATE
#define BAUD_RATE       115200 /* desired RS232 baud rate */
#endif

#ifndef UART_CLK
#define UART_CLK        UARTCLOCK/16 /* UART CLK input */
#endif

#define UART_DIVISOR    (UART_CLK/BAUD_RATE)

static const char* uart_chip_name[] = {
    [UART_UNKNOWN] = "unknown",
    [UART_16450]   = "16450",
    [UART_16550]   = "16550",
    [UART_16550A]  = "16550A",
    [UART_16750]   = "16750",
    [UART_16950]   = "16950",
    [UART_16950B]  = "16950B",
};

static uart_type_t uart_type = UART_UNKNOWN;

static void uart_icr_write(uint8_t offset, uint8_t value)
{
    uart_outb(UART_ADDRESS+UART_SCR, offset);
    uart_outb(UART_ADDRESS+UART_ICR, value);
}

static uint8_t uart_icr_read(uint8_t offset)
{
    uart_icr_write(UART_ACR, 0 | UART_ACR_ICRRD);     /* turn ON ACR[7] */
    uart_outb(UART_ADDRESS+UART_SCR, offset);
    uint8_t value = uart_inb(UART_ADDRESS+UART_ICR);
    uart_icr_write(UART_ACR, 0);                      /* turn off ACR[7] */
    return value;
}

void uart_init(void)
{
    bool autoflow;
    uint8_t LCR = UART_LCR_WLEN8;               /* 8N1 */
    uint8_t MCR = UART_MCR_DTR | UART_MCR_RTS;  /* set DTR, RTS */
    uint8_t FCR = UART_FCR_ENABLE_FIFO | UART_FCR_RXFIFO_TRIGH | UART_FCR_DEEP;

    /* Disable interrupts */
    uart_outb(UART_ADDRESS+UART_IER, 0);

    /* Check CTS bit in MSR: if high, hardware auto-flow control will be
     * enabled (for UARTs with this feature) */
    autoflow = uart_inb(UART_ADDRESS+UART_MSR) & 0x10;
    if(autoflow)
        MCR |= UART_MCR_AFE; /* set auto flow-control enable bit */

    /* Clear 1950 Extended Features Register (if present) */
    uart_outb(UART_ADDRESS+UART_LCR, UART_LCR_CONF_MODE_B);
    uart_outb(UART_ADDRESS+UART_EFR, 0);
    uart_outb(UART_ADDRESS+UART_LCR, LCR);

    /* Enable and reset FIFOs */ 
    uart_outb(UART_ADDRESS+UART_FCR, FCR | UART_FCR_CLEAR_RX_FIFO | UART_FCR_CLEAR_TX_FIFO);

    /* Examine IIR FIFO bits to determine chip type */
    switch (uart_inb(UART_ADDRESS+UART_IIR) & UART_IIR_FIFO_ENABLED)
    {
        case UART_IIR_FIFO_ENABLED_16450:       /* 00 */
            uart_type = UART_16450;
            break;
        case UART_IIR_FIFO_ENABLED_16550:       /* 10 */
            uart_type = UART_16550;
            break;
        case UART_IIR_FIFO_ENABLED_16550A:      /* 11 */
            uart_type = UART_16550A;
            break;
        default:                                /* 01 */
            uart_type = UART_UNKNOWN;
            break;
    }

    /* check for 16950 */
    if(uart_type == UART_16550A){
        uart_outb(UART_ADDRESS+UART_LCR, UART_LCR_CONF_MODE_B);
        if(uart_inb(UART_ADDRESS+UART_EFR) == 0){
            uint8_t id1 = 0, id2 = 0, id3 = 0, rev = 0;

            uart_outb(UART_ADDRESS+UART_EFR, UART_EFR_ECB | UART_EFR_RTS | (autoflow ? UART_EFR_CTS : 0));
            uart_outb(UART_ADDRESS+UART_LCR, LCR);

            id1 = uart_icr_read(UART_ID1);
            id2 = uart_icr_read(UART_ID2);
            id3 = uart_icr_read(UART_ID3);
            rev = uart_icr_read(UART_REV);

            if(id1 == 0x16 && id2 == 0xC9 && id3 == 0x50)
                uart_type = (rev == 3) ? UART_16950B : UART_16950;
        }else
            uart_outb(UART_ADDRESS+UART_LCR, LCR);
    }

    /* not 16950? check for 16750 */
    if(uart_type == UART_16550A) {
        /* try to enable the 64-byte FIFO */
        uart_outb(UART_ADDRESS+UART_LCR, UART_LCR_CONF_MODE_A); /* LCR bit 7 must be set first */
        uart_outb(UART_ADDRESS+UART_FCR, FCR);                  /* setting bit 5 enables 64 byte FIFO */
        uart_outb(UART_ADDRESS+UART_LCR, LCR);                  /* restore LCR */
        /* 16750 will set IIR bits 5, 6, 7 to indicate 64-byte FIFO enabled; 16550A sets 5, 6 only */
        if((uart_inb(UART_ADDRESS+UART_IIR) & UART_IIR_FIFO_ENABLED_16750) == UART_IIR_FIFO_ENABLED_16750)
            uart_type = UART_16750;
        /* NOTE 64-byte FIFO bit may be disabled by any write to FCR when LCR bit 7 is clear */
    }

    uart_outb(UART_ADDRESS+UART_LCR, UART_LCR_CONF_MODE_A); /* set DLAB to access divisor */
    uart_outb(UART_ADDRESS+0, UART_DIVISOR & 0xFF);    /* set divisor LSB */
    uart_outb(UART_ADDRESS+1, UART_DIVISOR >> 8);      /* set divisor MSB */
    uart_outb(UART_ADDRESS+UART_LCR, LCR);             /* restore LCR */
    uart_outb(UART_ADDRESS+UART_MCR, MCR);             /* set DTR, RTS and maybe AFE */
}

void uart_identify(void)
{
    uint8_t FCR = uart_inb(UART_ADDRESS+UART_IIR);             /* FCR/IIR */
    uint8_t LCR = uart_inb(UART_ADDRESS+UART_LCR);
    uint8_t MCR = uart_inb(UART_ADDRESS+UART_MCR);
    uint8_t IER = uart_inb(UART_ADDRESS+UART_IER);
    uint8_t MSR = uart_inb(UART_ADDRESS+UART_MSR);
    uint8_t SCR = uart_inb(UART_ADDRESS+UART_SCR);
    uint8_t LSR = uart_inb(UART_ADDRESS+UART_LSR);

    printf("UART: %s, IER=0x%02x IIR=0x%02x LCR=0x%02x MCR=0x%02x LSR=0x%02x MSR=0x%02x SCR=0x%02x\n", 
            uart_chip_name[uart_type], IER, FCR, LCR, MCR, LSR, MSR, SCR);

    return;
}

bool uart_write_ready(void)
{
    return uart_inb(UART_ADDRESS+UART_LSR) & UART_LSR_THRE ? true : false;
}

void uart_flush(void)
{
    while((uart_inb(UART_ADDRESS+UART_LSR) & (UART_LSR_THRE|UART_LSR_TEMT)) != (UART_LSR_THRE|UART_LSR_TEMT));
}

void uart_write_byte(char b)
{
    while(!(uart_inb(UART_ADDRESS+UART_LSR) & UART_LSR_THRE));
    uart_outb(UART_ADDRESS+UART_THR, b);
}

int uart_write_string(const char *str)
{
    int r = 0;

    while(*str){
        if(*str == '\n')
            uart_write_byte('\r');
        uart_write_byte(*(str++));
        r++;
    }

    return r;
}

bool uart_read_ready(void)
{
    return uart_inb(UART_ADDRESS+UART_LSR) & UART_LSR_DR ? true : false;
}

int uart_read_byte(void)
{
    if(uart_inb(UART_ADDRESS+UART_LSR) & UART_LSR_DR)
        return uart_inb(UART_ADDRESS+UART_RBR);
    else
        return -1;
}

uint8_t uart_read_byte_wait(void)
{
    while(!(uart_inb(UART_ADDRESS+UART_LSR) & UART_LSR_DR));
    return uart_inb(UART_ADDRESS+UART_RBR);
}

void uart_read_string(void *buffer, int count)
{
    char *p = buffer;
    char *end = p + count;

    while(p < end){
        while(!(uart_inb(UART_ADDRESS+UART_LSR) & UART_LSR_DR));
        *(p++) = uart_inb(UART_ADDRESS+UART_RBR);
    }
}

bool uart_check_cancel_key(void)
{
    int byte;

    byte = uart_read_byte();
    switch(byte){
        case 'q':
        case 'Q':
        case 0x1b: /* ESC */
            return true;
        default:
            return false;
    }
}
