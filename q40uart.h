#ifndef __Q40UART_DOT_H__
#define __Q40UART_DOT_H__

#include <stdbool.h>

void uart_init(void);
bool uart_write_ready(void);
void uart_write_byte(char b);
int uart_write_str(const char *str);

bool uart_read_ready(void);
int uart_read_byte(void); /* -1 if not ready */
uint8_t uart_read_byte_wait(void); /* waits if not ready */
void uart_read_string(void *buffer, int count);

#endif
