/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <uart.h>
#include <stdlib.h>

#if defined(TARGET_Q40) /* ISA based targets */

#include <q40/isa.h>
#define UART_ADDRESS    0x3f8
static inline uint8_t uart_inb(uint16_t port) { return isa_read_byte(port); }
static inline void uart_outb(uint16_t port, uint8_t val) { isa_write_byte(port, val); }

#define UARTCLOCK 1843200

#elif defined(TARGET_KISS) || defined(TARGET_MINI) /* ECB based targets */

#include <ecb/ecb.h>
#define UART_ADDRESS    (MFPIC_UART)
static inline uint8_t uart_inb(uint16_t port) { return ecb_read_byte(port); }
static inline void uart_outb(uint16_t port, uint8_t val) { ecb_write_byte(port, val); }

#define UARTCLOCK MFPIC_UART_CLK

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

char* UARTArray[] = {
    "unknown",
    "16450",
    "16550",
    "16550A",
    "16750",
    "16950",
    "16950B"
};

static void serial_icr_write(uint8_t offset, uint8_t value)
{
    uart_outb(UART_ADDRESS+UART_SCR, offset);
    uart_outb(UART_ADDRESS+UART_ICR, value);
}

static uint8_t serial_icr_read(uint8_t offset)
{
    uint8_t value;

    serial_icr_write(UART_ACR, 0 | UART_ACR_ICRRD);	// turn ON ACR[7]

    uart_outb(UART_ADDRESS+UART_SCR, offset);
    value = uart_inb(UART_ADDRESS+UART_ICR);

    serial_icr_write(UART_ACR, 0);	// turn off ACR[7]

    return value;
}

void uart_init(void)
{
    bool autoflow = true;

    uint8_t FCR = 0xE7;
    uint8_t LCR = UART_LCR_WLEN8;	// 8N1
    uint8_t MCR = UART_MCR_DTR|UART_MCR_RTS;	// DTR, RTS

    uint8_t MSR = uart_inb(UART_ADDRESS+UART_MSR);
    if ((MSR & 0x10) != 0x10) {
        autoflow = false;
    }

    uint8_t uart_id = 0;

    uart_outb(UART_ADDRESS+UART_IER, 0);                /* disable interrupts */

    uart_outb(UART_ADDRESS+UART_LCR, UART_LCR_CONF_MODE_B);
    uart_outb(UART_ADDRESS+UART_EFR, 0);
    uart_outb(UART_ADDRESS+UART_LCR, 0x00);             /* clear DLAB */

    uart_outb(UART_ADDRESS+UART_FCR, UART_FCR_ENABLE_FIFO);

    switch (uart_inb(UART_ADDRESS+UART_IIR) & UART_IIR_FIFO_ENABLED)
    {
        case UART_IIR_FIFO_ENABLED_16450:	// 00
            uart_id = 1;
            break;
        case UART_IIR_FIFO_ENABLED_16550:	// 10
            uart_id = 2;
            break;
        case UART_IIR_FIFO_ENABLED_16550A:	// 11
        {
            uart_id = 3;

            uart_outb(UART_ADDRESS+UART_LCR, UART_LCR_CONF_MODE_B);
            if (uart_inb(UART_ADDRESS+UART_EFR) == 0)
            {
                uint8_t id1 = 0, id2 = 0, id3 = 0, rev = 0;

                if (autoflow == true)
                    uart_outb(UART_ADDRESS+UART_EFR, UART_EFR_ECB|UART_EFR_RTS|UART_EFR_CTS);
                else
                    uart_outb(UART_ADDRESS+UART_EFR, UART_EFR_ECB|UART_EFR_RTS);
                uart_outb(UART_ADDRESS+UART_LCR, 0x00);

                id1 = serial_icr_read(UART_ID1);
                id2 = serial_icr_read(UART_ID2);
                id3 = serial_icr_read(UART_ID3);
                rev = serial_icr_read(UART_REV);

                if (id1 == 0x16 && id2 == 0xC9 && id3 == 0x50) {
                    uart_id = 5;
                    if (rev == 3) uart_id++; // found a 16C950 rev.B

                    MCR = 0x3; // set DTR and RTS
                }
            }
            break;
        }
        default:				// 01
            uart_id = 0;
            break;
    }

    // check for 16C750
    if (uart_id == 3)
    {
        uart_outb(UART_ADDRESS+UART_LCR, UART_LCR_CONF_MODE_B);     /* set needed to activate 64 byte FIFO */
        uart_outb(UART_ADDRESS+UART_FCR, UART_FCR_ENABLE_FIFO | UART_FCR7_64BYTE);     /* set bit 5 for FIFO 64 bytes enable */
        uart_outb(UART_ADDRESS+UART_LCR, 0x00);     /* clear LCR */

        if ((uart_inb(UART_ADDRESS+UART_IIR) & 0xe0) == 0xe0)
            uart_id = 4;
    }

    if ((uart_id == 3 || uart_id == 4) && autoflow == true)
        MCR |= UART_MCR_AFE; // set also the AFE bit

    uart_outb(UART_ADDRESS+UART_LCR, UART_LCR_DLAB);   /* set DLAB to access divisor */
    uart_outb(UART_ADDRESS+0, UART_DIVISOR & 0xFF);    /* set divisor LSB */
    uart_outb(UART_ADDRESS+1, UART_DIVISOR >> 8);      /* set divisor MSB */
    uart_outb(UART_ADDRESS+UART_FCR, FCR);             /* enable and clear RX/TX FIFOs (if present) */
    uart_outb(UART_ADDRESS+UART_LCR, LCR);             /* clear DLAB, set 8 bit data, 1 stop bit, no parity */
    uart_outb(UART_ADDRESS+UART_MCR, MCR);             /* set DTR, RTS and maybe AFE */

    FCR = uart_inb(UART_ADDRESS+UART_IIR); // FCR/IIR
    LCR = uart_inb(UART_ADDRESS+UART_LCR);
    MCR = uart_inb(UART_ADDRESS+UART_MCR);

    uint8_t IER = uart_inb(UART_ADDRESS+UART_IER);
            MSR = uart_inb(UART_ADDRESS+UART_MSR);
    uint8_t SCR = uart_inb(UART_ADDRESS+UART_SCR);
    uint8_t LSR = uart_inb(UART_ADDRESS+UART_LSR);

    printf("\n\nUART: %s, IER=0x%02x IIR=0x%02x LCR=0x%02x MCR=0x%02x LSR=0x%02x MSR=0x%02x SCR=0x%02x \n", UARTArray[uart_id], IER, FCR, LCR, MCR, LSR, MSR, SCR);
    if (autoflow == false)
        puts("detected /CTS HIGH, starting without AUTOFLOW ENABLE\n");

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
