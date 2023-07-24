#include "q40hw.h"
#include "q40isa.h"
#include "q40uart.h"
#include <stdlib.h>

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

void boot_q40(void)
{
    // doing this will crash the machine ... stack problem?
    q40_led(0);

    /* TODO: ISA bus reset */

    /* TODO: master chip full initialisation */

    uart_init();
    uart_write_str(copyright_msg);
    printf("printf test 1\n");
    printf("printf test 2: %s\n", "it works!");
    printf("test value = 0x%08x\n", 0x1234ABCD);
    q40_led(1);
}
