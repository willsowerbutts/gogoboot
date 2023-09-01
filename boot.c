/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <q40/hw.h>
#include <disk.h>
#include <q40/isa.h>
#include <uart.h>
#include <net.h>
#include <cli.h>
#include <init.h>
#include <version.h>
#include <tinyalloc.h>

extern const char copyright_msg[];
extern const char text_start, text_size;
extern const char rodata_start, rodata_size;
extern const char data_start, data_load_start, data_size;
extern const char bss_start, bss_size, bss_end;

static void report_segment(const char *name, int start, int size, int load)
{
    char unit;
    int dec;

    dec = size;
    if(dec >= 1024*1024){
        dec >>= 10;
        unit = 'M';
    }else{
        unit = 'K';
    }
    dec *= 10;
    dec += 512; /* round to nearest */
    dec >>= 10;

    // printf("    text        0     A916   42.3 KB\n");
    printf("%8s  %8x  %8x  %3d.%d %cB", name, start, size, dec / 10, dec % 10, unit);
    if(load)
        printf("  load from %x", load);
    printf("\n");
}

void report_memory_layout(void)
{
    printf(" segment     start    length\n");
    report_segment("text",   (int)&text_start,   (int)&text_size, 0); 
    report_segment("rodata", (int)&rodata_start, (int)&rodata_size, 0); 
    report_segment("data",   (int)&data_start,   (int)&data_size, (int)&data_load_start);
    report_segment("bss",    (int)&bss_start,    (int)&bss_size, 0);
    report_segment("(free)", (int)&bss_end,      (int)heap_base - (int)&bss_end, 0);
    report_segment("heap",   (int)heap_base,     (int)heap_size, 0);
    report_segment("stack",  (int)stack_base,    (int)stack_size, 0);
}

#define MAXHEAP (2 << 20) /* 2MB */

static void heap_init(void)
{
    // this is an attempt to leave enough space
    // above .data for us to load a sizeable
    // program (ie, linux). it's not ideal.
    // shame that talloc cannot allocate from
    // the top downwards.

    heap_size = ram_size / 4;  /* not more than 25% of RAM */
    if(heap_size > MAXHEAP)    /* and not too much */
        heap_size = MAXHEAP;

    heap_base = ram_size - heap_size;
    heap_size = stack_base - heap_base;
    ta_init((void*)heap_base, (void*)heap_base + heap_size - 1, heap_size > (200*1024) ? 2048 : 256, 16, 4);
}

void report_ram_installed(void)
{
    int shift;
    char unit;

    heap_init();

    if(ram_size >= 8*1024*1024){
        shift = 20;
        unit = 'M';
    }else{
        shift = 10;
        unit = 'K';
    }

    printf("RAM installed: %ld %cB\n", (ram_size + (1 << shift) - 1)>>shift, unit);
    report_memory_layout();
}

void gogoboot(void)
{
    early_init();
    uart_init();
    puts(copyright_msg);
    printf("Version %s\n", software_version_string);
    heap_init();
    report_ram_installed();
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
