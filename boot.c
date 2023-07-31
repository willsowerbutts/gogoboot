#include <stdlib.h>
#include <stdbool.h>
#include "tinyalloc.h"
#include "q40hw.h"
#include "q40ide.h"
#include "q40isa.h"
#include "q40uart.h"
#include "net.h"
#include "cli.h"

extern const char copyright_msg[];
bool networking;

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
 * DONE - 68K exception handler
 * DONE   - trap #0 required for SMSQ/E -- should put us in supervisor mode, so basically a NOP.
 * DONE - ISA bus reset
 * DONE - configure the other master chip's registers -- interrupt control?
 * DONE - NE2000 driver
 * DONE - NE2000 driver to use 16-bit transfers
 * DONE - back to back transmits do not work?
 * DONE - during tx of short packet, we're dumping crap out of the buffer too -- fix and replace with 00s
 * - DHCP -- perform in the background
 * - would be nice if getline in the CLI somehow recovers after we overwrite it ...
 * - set and store environment vars in NVRAM (we have malloc now!)
 * - TFTP protocol to read/write files on disk (look into extensions for larger block size, pipeline, watch out for card memory limit)
 * - linux ne2000 driver: stop interrupt probing (=crashes machine)
 * - SOFTROM feature clone, so we can test new ROMs (higher baud rate, build as builtin cmd or ELF executable?)
 * - ultimately target a port back to kiss-68030?
 * - store ROM config in RTC NVRAM? MAC address, serial port speed, etc? store as series of strings; cksum at start.
 * - equivalent of LRESPR so I can boot SMSQ/E image from disk -- started, but crashes. Write to SMSQ/E maintainer?
 * - serial interrupts (at least on receive, pretty much required for zmodem to disk I expect)
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

    if((int)(&bss_start) + (int)(&bss_size) >= 256*1024){
        printf("!!! WARNING !!! BSS may conflict with kernel load address!\n");
    }
}

#define MAXHEAP (4 << 20) /* 4MB */

static unsigned int heap_init(void)
{
    int heap;
    void *base;

    // this is an attempt to leave enough space
    // above .data for us to load a sizeable
    // program (ie, linux). it's not ideal.
    // shame that talloc cannot allocate from
    // the top downwards.

    heap = ram_size / 4;  /* not more than 25% of RAM */
    if(heap > MAXHEAP)    /* and not too much */
        heap = MAXHEAP;

    base = (void*)ram_size - heap;
    ta_init(base, (void*)ram_size-1, 256, 16, 8);

    return (unsigned int)base;
}

void boot_q40(void)
{
    q40_led(false);
    q40_isa_reset();
    uart_init();

    printf(copyright_msg);
    report_linker_layout();

    printf("RAM installed: ");
    q40_measure_ram_size();
    unsigned int heap_base = heap_init();
    printf("%d MB, %d MB heap at 0x%08x\n", ram_size>>20, (ram_size-heap_base)>>20, heap_base);

    printf("Setup interrupts: ");
    q40_setup_interrupts(); /* do this early to get timers ticking */
    printf("done\n");

    printf("Initialise RTC: ");
    q40_rtc_init();

    printf("\nInitialise video: ");
    q40_graphics_init(3);
    printf("done\n");

    q40_ide_init();

    printf("Initialise ethernet: ");
    eth_init();
    net_init(); // do this only after the ethernet is up

    q40_led(true);

    command_line_interpreter();

    q40_led(false);
    puts("[halted]");
}
