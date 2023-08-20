/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <kiss/ecb.h>
#include <uart.h>

#define UART_ADDRESS    0x3f8
#define BAUD_RATE       115200
#define UART_DIVISOR    (115200/BAUD_RATE)

void uart_init(void)
{
    ecb_write_byte(UART_ADDRESS+UART_IER, 0);                /* disable interrupts */
    ecb_write_byte(UART_ADDRESS+UART_LCR, 0x80);             /* set DLAB to access divisor */
    ecb_write_byte(UART_ADDRESS+0, UART_DIVISOR & 0xFF);     /* set divisor LSB */
    ecb_write_byte(UART_ADDRESS+1, UART_DIVISOR >> 8);       /* set divisor MSB */
    ecb_write_byte(UART_ADDRESS+UART_LCR, 0x03);             /* clear DLAB, set 8 bit data, 1 stop bit, no parity */
    ecb_write_byte(UART_ADDRESS+UART_FCR, 0x07);             /* enable and clear RX/TX FIFOs (if present) */
    ecb_write_byte(UART_ADDRESS+UART_MCR, 0x03);             /* set DTR, RTS */
}

bool uart_write_ready(void)
{
    return ecb_read_byte(UART_ADDRESS+UART_LSR) & UART_LSR_THRE ? true : false;
}

void uart_write_byte(char b)
{
    while(!(ecb_read_byte(UART_ADDRESS+UART_LSR) & UART_LSR_THRE));
    ecb_write_byte(UART_ADDRESS+UART_THR, b);
}

int uart_write_str(const char *str)
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
    return ecb_read_byte(UART_ADDRESS+UART_LSR) & UART_LSR_DR ? true : false;
}

int uart_read_byte(void)
{
    if(ecb_read_byte(UART_ADDRESS+UART_LSR) & UART_LSR_DR)
        return ecb_read_byte(UART_ADDRESS+UART_RBR);
    else
        return -1;
}

uint8_t uart_read_byte_wait(void)
{
    while(!(ecb_read_byte(UART_ADDRESS+UART_LSR) & UART_LSR_DR));
    return ecb_read_byte(UART_ADDRESS+UART_RBR);
}

void uart_read_string(void *buffer, int count)
{
    char *p = buffer;
    char *end = p + count;

    while(p < end){
        while(!(ecb_read_byte(UART_ADDRESS+UART_LSR) & UART_LSR_DR));
        *(p++) = ecb_read_byte(UART_ADDRESS+UART_RBR);
    }
}
