/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <stdbool.h>
#include <tinyalloc.h>
#include <q40/hw.h>
#include <disk.h>
#include <q40/isa.h>
#include <uart.h>
#include <net.h>
#include <cli.h>
#include <init.h>
#include <version.h>

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
 * DONE - DHCP -- perform in the background
 * DONE - Emulator - add ne2000 -- https://github.com/OBattler/PCem-X/blob/master/PCem/ne2000.c
 * DONE - ARP cache and reply to queries for our MAC
 * DONE - ARP resolution
 * DONE - what is behind these "ne2000: rx too big" errors?! going too fast on the ISA bus!
 * DONE - test TFTP robustness (large transfers = file ends up the wrong size!)
 * DONE - FAT FS long file name support (should be easy now we have malloc?)
 * DONE - SOFTROM feature clone, so we can test new ROMs (higher baud rate, build as builtin cmd or ELF executable?)
 * DONE - test my SOFTROM tool at 115200bps -- consider changing everything to use this rate?
 * DONE - ls sorts into order (switchable?)
 * DONE - autoexec (with abort key)
 * DONE - kiss: copy ROM -> RAM and run from RAM
 * DONE - kiss: download new image over UART / load from disk, "reboot" into it ("softrom")
 * DONE - kiss: cannot write to disk? looks like memory is being corrupted, perhaps?
 * - kiss: ? in-system flash reprogramming utility (based on flash4)
 * - can we share more IDE driver code?
 * - move heap to be in low memory (and smaller?), set kernel load address dynamically based on computed highest used address?
 * - ne2000 driver to work with other cards (we have 3; it works with exactly 1!)
 * - TFTP write mode
 * - TFTP server mode
 * - DNS?
 * DONE - TFTP client protocol to read/write files on disk (look into extensions for larger block size, pipeline, watch out for window > ethernet card memory limit)
 * - TFTP server protocol too? wouldn't be hard!
 * - would be nice if getline in the CLI somehow recovers after we overwrite it ...
 * - set and store environment vars in NVRAM (we have malloc now!)
 *   - store ROM config in RTC NVRAM? serial port speed, boot script filename, etc? store as series of strings; cksum at end. store backwards in nvram.
 *   - printenv, set, save commands
 * DONE - linux ne2000 driver: stop interrupt probing (=crashes machine)
 * DONE - build system: multiple targets (maybe use something better than a makefile?)
 * DONE   - port back to kiss-68030
 * DONE     - 030 cache modes
 * DONE     - ns202 interrupts
 *     - move int handlers -> C?
 * - add "linux" command - make loading a kernel an explicit command (linux [filename] [-i initrd] [kcmdline])
 * - add LRESPR command - so I can boot SMSQ/E image from disk -- works with older (from ROMs) but fails with newer. Write to SMSQ/E maintainer?
 * - serial interrupts (at least on receive? pretty much required for zmodem to disk I expect)
 * - ZMODEM! https://github.com/spk121/libzmodem ?
 * - gzip/gunzip?
 */

extern const char copyright_msg[];
extern const char text_start, text_size;
extern const char rodata_start, rodata_size;
extern const char data_start, data_load_start, data_size;
extern const char bss_start, bss_size;
#ifdef TARGET_KISS
extern const char stack_start, stack_size;
#endif

void report_linker_layout(void)
{
    printf("  .text    0x%08x length 0x%08x\n", (int)&text_start, (int)&text_size); 
    printf("  .rodata  0x%08x length 0x%08x\n", (int)&rodata_start, (int)&rodata_size); 
#ifdef TARGET_KISS
    printf("  .stack   0x%08x length 0x%08x\n", (int)&stack_start, (int)&stack_size); 
#endif
    printf("  .data    0x%08x length 0x%08x (load from 0x%08x)\n", (int)&data_start, (int)&data_size, (int)&data_load_start);
    printf("  .bss     0x%08x length 0x%08x\n\n", (int)&bss_start, (int)&bss_size);

    if((int)(&bss_start) + (int)(&bss_size) >= 256*1024){
        printf("!!! WARNING !!! BSS conflict with kernel load address!\n");
    }
}

#define MAXHEAP (2 << 20) /* 2MB */

static uint32_t heap_init(void)
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
    ta_init(base, (void*)ram_size-1, 2048, 16, 8);

    return (uint32_t)base;
}

void gogoboot(void)
{
    uint32_t heap_base;

    early_init();

    uart_init();

    printf(copyright_msg);
    report_linker_layout();
    printf("Version %s\n", software_version_string);

    printf("RAM installed: ");
    measure_ram_size();
    heap_base = heap_init();
    printf("%ld MB, %ld MB heap at 0x%08lx\n", ram_size>>20, (ram_size-heap_base)>>20, heap_base);

    printf("Setup interrupts: ");
    setup_interrupts(); /* do this early to get timers ticking */
    printf("done\n");

    printf("Initialise RTC: ");
    rtc_init();

    disk_init();

    target_hardware_init();

    printf("Initialise ethernet: ");
    net_init();
    if(eth_init()){
        dhcp_init();
    }

    command_line_interpreter();

    // should not get here
    puts("[halted]");
}
