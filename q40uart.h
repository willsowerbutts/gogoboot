#ifndef __Q40UART_DOT_H__
#define __Q40UART_DOT_H__

void uart_init(void);
void uart_write_byte(char b);
int uart_write_str(const char *str);

#endif
