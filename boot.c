#include <stdlib.h>
#include <stdbool.h>
#include "q40hw.h"
#include "q40ide.h"
#include "q40isa.h"
#include "q40uart.h"

extern const char copyright_msg[];
void command_line_interpreter(void);

/* TODO:
 * DONE - 040 cache modes
 * DONE - debug odd crashes (SOFTROM bug)
 * DONE - printf
 * DONE - memcpy
 * DONE - video mode?
 * DONE - blank the screen
 * DONE - measure installed RAM
 * DONE - IDE interface 
 * DONE - command line interface
 * DONE - FAT filesystem
 * DONE - linux loader
 * DONE - some sort of timer (add interrupt support, then use the timer tick?)
 * - 68K exception handler
 *   - trap #0 required for SMSQ/E -- should put us in supervisor mode, so basically a NOP.
 * - ISA bus reset
 * - configure the other master chip's registers -- interrupt control?
 * - SOFTROM feature clone, so we can test new ROMs (higher baud rate, build as builtin cmd or ELF executable?)
 * - NE2000 driver
 * - TFTP protocol to read/write files on disk (look into extensions for larger block size, pipeline, watch out for card memory limit)
 * - ultimately target a port back to kiss-68030?
 * - store ROM config in RTC NVRAM? MAC address, serial port speed, etc? store as series of strings; cksum at start.
 * - equivalent of LRESPR so I can boot SMSQ/E image from disk -- started, but crashes. Write to SMSQ/E maintainer?
 * - serial interrupts (at least on receive)
 * - ZMODEM! https://github.com/spk121/libzmodem ?
 * - gzip/gunzip?
 */

extern char text_start, text_size;
extern char rodata_start, rodata_size;
extern char data_start, data_load_start, data_size;
extern char bss_start, bss_size;

void report_linker_layout(void)
{
    printf("  .text    0x%08x length 0x%08x\n", (int)&text_start, (int)&text_size); 
    printf("  .rodata  0x%08x length 0x%08x\n", (int)&rodata_start, (int)&rodata_size); 
    printf("  .data    0x%08x length 0x%08x (load from 0x%08x)\n", (int)&data_start, (int)&data_size, (int)&data_load_start);
    printf("  .bss     0x%08x length 0x%08x\n\n", (int)&bss_start, (int)&bss_size);
}

void boot_q40(void)
{
    q40_led(false);
    q40_isa_reset();
    uart_init();

    uart_write_str(copyright_msg);

    report_linker_layout();

    printf("Setup interrupts: ");
    q40_setup_interrupts(); /* do this VERY early: timers depend upon it */
    printf("done\n");

    printf("Initialise RTC: ");
    q40_rtc_init();
    printf("done\n");

    q40_measure_ram_size();
    printf("RAM installed: %d MB\n", ram_size>>20);

    printf("Initialise video: ");
    q40_graphics_init(3);
    printf("done\n");

    q40_ide_init();

    q40_led(true);

    command_line_interpreter();

    q40_led(false);
    puts("[halted]");
}
