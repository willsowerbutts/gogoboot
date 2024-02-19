#ifndef __GOGOBOOT_UART_DOT_H__
#define __GOGOBOOT_UART_DOT_H__

#include <stdbool.h>
#include <types.h>

void uart_init(void);
void uart_flush(void);
bool uart_write_ready(void);
void uart_write_byte(char b);
int uart_write_string(const char *str);

bool uart_read_ready(void);
int uart_read_byte(void); /* -1 if not ready */
uint8_t uart_read_byte_wait(void); /* waits if not ready */
void uart_read_string(void *buffer, int count);
bool uart_check_cancel_key(void);

/* 16550 UART hardware registers */
#define UART_THR        0       /* transmit holding register */
#define UART_RBR        0       /* receive buffer register */
#define UART_IER        1       /* interrupt enable register */
#define UART_IIR        2       /* interrupt identification register (read) */
#define UART_FCR        2       /* FIFO control register (write) */
#define UART_LCR        3       /* line control register */
#define UART_MCR        4       /* modem control register */
#define UART_LSR        5       /* line status register */
#define UART_ICR        5       /* index control register */
#define UART_MSR        6       /* modem status register */
#define UART_SCR        7       /* scratch register */

/* 16C950 UART hardware register */
#define UART_ACR        0       /* Additional Control Register */
#define UART_EFR        2       /* Extended Features Register */
#define UART_ID1        8       /* UART ID1 */
#define UART_ID2        9       /* UART ID2 */
#define UART_ID3        10      /* UART ID3 */
#define UART_REV        11      /* UART Revision */

#define UART_ACR_ICRRD  0x40    /* ICR Read enable */

#define UART_FCR_ENABLE_FIFO   0x01 /* Enable the FIFO */
#define UART_LSR_DR     0x01    /* LSR: data ready bit */

#define UART_EFR_ECB    0x10    /* LSR: Enhanced control bit */
#define UART_EFR_RTS    0x40    /* RTS flow control */
#define UART_EFR_CTS    0x80    /* CTS flow control */

#define UART_LSR_THRE   0x20    /* LSR: transmit holding register empty */
#define UART_LSR_TEMT   0x40    /* LSR: transmitter empty */

#define UART_LCR_DLAB          0x80 /* Divisor latch access bit */
#define UART_LCR_CONF_MODE_A   UART_LCR_DLAB /* Configutation mode A */
#define UART_LCR_CONF_MODE_B   0xBF /* Configutation mode B */
#define UART_LCR_WLEN8         0x03 /* Wordlength: 8 bits */

#define UART_MCR_DTR   0x01 /* DTR complement */
#define UART_MCR_RTS   0x02 /* RTS complement */
#define UART_MCR_OUT1  0x04 /* Out1 complement */
#define UART_MCR_OUT2  0x08 /* Out2 complement */
#define UART_MCR_LOOP  0x10 /* Enable loopback test mode */
#define UART_MCR_AFE   0x20 /* Enable auto-RTS/CTS (TI16C550C/TI16C750) */

#define UART_FCR7_64BYTE 0x20
#define UART_IIR_FIFO_ENABLED	0xc0 /* FIFOs enabled / port type identification */
#define UART_IIR_FIFO_ENABLED_16450	0x00	/* 16450: no FIFO */
#define UART_IIR_FIFO_ENABLED_16550	0x80	/* 16550: (broken/unusable) FIFO */
#define UART_IIR_FIFO_ENABLED_16550A	0xc0	/* 16550A: FIFO enabled */
#define UART_IIR_FIFO_ENABLED_16750	0xe0	/* 16750: 64 bytes FIFO enabled */


#endif
