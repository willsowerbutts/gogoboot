#define UART_ADDRESS    0x3f8
#define UART_DIVISOR    (115200/9600)

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

unsigned char isa_read_byte(unsigned short addr)
{
    volatile unsigned char *isa_addr = (unsigned char *)(0xff400001 + (addr << 2));
    return *(isa_addr);
}

void isa_write_byte(unsigned short addr, unsigned char val)
{
    volatile unsigned char *isa_addr = (unsigned char *)(0xff400001 + (addr << 2));
    *(isa_addr) = val;
}

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

void cboot(void)
{
    uart_init();
    uart_write_str("hello, world!\n");
}
