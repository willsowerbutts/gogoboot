/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <disk.h>
#include <uart.h>
#include <net.h>
#include <cli.h>
#include <init.h>
#include <rtc.h>
#include <version.h>
#include <tinyalloc.h>

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
    if(load && load != start)
        printf("  load from %x", load);
    printf("\n");
}

void report_memory_layout(void)
{
    uint32_t free_ram_top;

    if(heap_base > ram_size)
        free_ram_top = ram_size;
    else
        free_ram_top = heap_base;

    printf(" segment     start    length\n");
    report_segment("text",   (int)&text_start,   (int)&text_size, 0); 
    report_segment("rodata", (int)&rodata_start, (int)&rodata_size, 0); 
    report_segment("data",   (int)&data_start,   (int)&data_size, (int)&data_load_start);
    report_segment("bss",    (int)&bss_start,    (int)&bss_size, 0);
    report_segment("(free)", (int)bounce_below_addr, (int)free_ram_top - (int)bounce_below_addr, 0);
    report_segment("heap",   (int)heap_base,     (int)heap_size, 0);
    report_segment("stack",  (int)stack_base,    (int)stack_size, 0);
}

static void heap_init(void)
{
    ta_init((void*)heap_base, (void*)heap_base + heap_size - 1, heap_size > (200*1024) ? 2048 : 256, 16, 4);
}

void report_ram_installed(void)
{
    int shift;
    char unit;

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

void report_current_time(void)
{
    rtc_time_t now;
    rtc_read_clock(&now);
    printf("%04d-%02d-%02d %02d:%02d:%02d\n",
            now.year, now.month, now.day, now.hour, now.minute, now.second);
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
    report_current_time();

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
