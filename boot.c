#include "q40hw.h"
#include "q40isa.h"
#include "q40uart.h"

extern const char copyright_msg[];

/* TODO:
 * - printf
 * - memcpy
 * - video mode?
 * - ISA bus reset
 * - configure the master chip's registers
 * - blank the screen
 * - some sort of timer (just use the hardware interrupt tick?)
 * - IDE interface + FAT filesystem
 * - linux loader
 * - NE2000 driver
 * - ultimately target a port back to kiss-68030?
 * DONE - 040 cache modes
 */

void print_hex(unsigned int a)
{
    char b;
    char buf[12];
    char *p = buf + 11;

    *p = 0;
    do{
        b = a & 0xF;
        a = a >> 4;
        p--;
        if(b < 10)
            *p = '0' + b;
        else
            *p = 'A' + b - 10;
    }while(a);

    *(--p)='x';
    *(--p)='0';

    uart_write_str(p);
}

void boot_q40(void)
{
    // doing this will crash the machine ... stack problem?
    q40_led(0);
    q40_led(1);

    /* TODO: ISA bus reset */

    /* TODO: master chip full initialisation */

    uart_init();
    uart_write_str(copyright_msg);
    uart_write_str("test value = ");
    print_hex(0x1234ABCD);
    uart_write_str("\n");

}
