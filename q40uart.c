#include "q40isa.h"
#include "q40uart.h"

#define UART_ADDRESS    0x3f8
#define UART_DIVISOR    (115200/57600)

#define UART_THR        0       /* transmit holding register */
#define UART_RBR        0       /* receive buffer register */
#define UART_IER        1       /* interrupt enable register */
#define UART_IIR        2       /* interrupt identification register (read) */
#define UART_FCR        2       /* FIFO control register (write) */
#define UART_LCR        3       /* line control register */
#define UART_MCR        4       /* modem control register */
#define UART_LSR        5       /* line status register */
#define UART_MSR        6       /* modem status register */
#define UART_SCR        7       /* scratch register */

#define UART_LSR_DR     0x01    /* LSR: data ready bit */
#define UART_LSR_THRE   0x20    /* LSR: transmit holding register empty */

void uart_init(void)
{
    isa_write_byte(UART_ADDRESS+UART_IER, 0);                /* disable interrupts */
    isa_write_byte(UART_ADDRESS+UART_LCR, 0x80);             /* set DLAB to access divisor */
    isa_write_byte(UART_ADDRESS+0, UART_DIVISOR & 0xFF);     /* set divisor LSB */
    isa_write_byte(UART_ADDRESS+1, UART_DIVISOR >> 8);       /* set divisor MSB */
    isa_write_byte(UART_ADDRESS+UART_LCR, 0x03);             /* clear DLAB, set 8 bit data, 1 stop bit, no parity */
    isa_write_byte(UART_ADDRESS+UART_FCR, 0x07);             /* enable and clear RX/TX FIFOs (if present) */
    isa_write_byte(UART_ADDRESS+UART_MCR, 0x03);             /* set DTR, RTS */
}

bool uart_write_ready(void)
{
    return isa_read_byte(UART_ADDRESS+UART_LSR) & UART_LSR_THRE ? true : false;
}

void uart_write_byte(char b)
{
    while(!(isa_read_byte(UART_ADDRESS+UART_LSR) & UART_LSR_THRE));
    isa_write_byte(UART_ADDRESS+UART_THR, b);
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
    return isa_read_byte(UART_ADDRESS+UART_LSR) & UART_LSR_DR ? true : false;
}

int uart_read_byte(void)
{
    if(isa_read_byte(UART_ADDRESS+UART_LSR) & UART_LSR_DR)
        return isa_read_byte(UART_ADDRESS+UART_RBR);
    else
        return -1;
}

uint8_t uart_read_byte_wait(void)
{
    while(!(isa_read_byte(UART_ADDRESS+UART_LSR) & UART_LSR_DR));
    return isa_read_byte(UART_ADDRESS+UART_RBR);
}
