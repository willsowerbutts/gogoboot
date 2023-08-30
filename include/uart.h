#ifndef __GOGOBOOT_UART_DOT_H__
#define __GOGOBOOT_UART_DOT_H__

#include <types.h>

void uart_init(void);
bool uart_write_ready(void);
void uart_write_byte(char b);
int uart_write_str(const char *str);

bool uart_read_ready(void);
int uart_read_byte(void); /* -1 if not ready */
uint8_t uart_read_byte_wait(void); /* waits if not ready */
void uart_read_string(void *buffer, int count);

/* 16550 UART hardware registers */
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

#endif
