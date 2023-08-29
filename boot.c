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

extern const char copyright_msg[];
extern const char text_start, text_size;
extern const char rodata_start, rodata_size;
extern const char data_start, data_load_start, data_size;
extern const char bss_start, bss_size;
extern const char stack_start, stack_size;

void report_linker_layout(void)
{
    printf("  .text    0x%08x length 0x%08x\n", (int)&text_start, (int)&text_size); 
    printf("  .rodata  0x%08x length 0x%08x\n", (int)&rodata_start, (int)&rodata_size); 
    printf("  .stack   0x%08x length 0x%08x\n", (int)&stack_start, (int)&stack_size); 
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
    ta_init(base, (void*)ram_size-1, heap > (256*1024) ? 2048 : 256, 16, 4);

    return (uint32_t)base;
}

void mem_init(void)
{
    uint32_t heap_base;
    int shift;
    char unit;

    printf("RAM installed: ");
    measure_ram_size();
    heap_base = heap_init();

    if(ram_size >= 4*1024*1024){
        shift = 20;
        unit = 'M';
    }else{
        shift = 10;
        unit = 'K';
    }

    printf("%ld %cB, %ld %cB heap at 0x%08lx\n", 
            ram_size>>shift, unit, 
            (ram_size-heap_base)>>shift, unit, 
            heap_base);
}

void gogoboot(void)
{

    early_init();

    uart_init();

    printf(copyright_msg);
    report_linker_layout();
    printf("Version %s\n", software_version_string);

    mem_init();

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
