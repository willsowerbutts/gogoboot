/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <uart.h>

#if defined(TARGET_Q40) /* ISA based targets */

#include <q40/isa.h>
#define UART_ADDRESS    0x3f8
static inline uint8_t uart_inb(uint16_t port) { return isa_read_byte(port); }
static inline void uart_outb(uint16_t port, uint8_t val) { isa_write_byte(port, val); }

#elif defined(TARGET_KISS) || defined(TARGET_MINI) /* ECB based targets */

#include <ecb/ecb.h>
#define UART_ADDRESS    (MFPIC_UART)
static inline uint8_t uart_inb(uint16_t port) { return ecb_read_byte(port); }
static inline void uart_outb(uint16_t port, uint8_t val) { ecb_write_byte(port, val); }

#else
#pragma error update uart.c for your target
#endif

#ifndef BAUD_RATE
#define BAUD_RATE       115200 /* desired RS232 baud rate */
#endif

#ifndef UART_CLK
#define UART_CLK        115200 /* UART CLK input */
#endif

#define UART_DIVISOR    (UART_CLK/BAUD_RATE)

void uart_init(void)
{
    uart_outb(UART_ADDRESS+UART_IER, 0);                /* disable interrupts */
    uart_outb(UART_ADDRESS+UART_LCR, 0x80);             /* set DLAB to access divisor */
    uart_outb(UART_ADDRESS+0, UART_DIVISOR & 0xFF);     /* set divisor LSB */
    uart_outb(UART_ADDRESS+1, UART_DIVISOR >> 8);       /* set divisor MSB */
    uart_outb(UART_ADDRESS+UART_LCR, 0x03);             /* clear DLAB, set 8 bit data, 1 stop bit, no parity */
    uart_outb(UART_ADDRESS+UART_FCR, 0x07);             /* enable and clear RX/TX FIFOs (if present) */
    uart_outb(UART_ADDRESS+UART_MCR, 0x03);             /* set DTR, RTS */
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
