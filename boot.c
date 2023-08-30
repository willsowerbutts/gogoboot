/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
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
    printf("  .stack   0x%08x length 0x%08x\n", (int)stack_base, STACK_SIZE);
    printf("  .data    0x%08x length 0x%08x (load from 0x%08x)\n", (int)&data_start, (int)&data_size, (int)&data_load_start);
    printf("  .bss     0x%08x length 0x%08x\n\n", (int)&bss_start, (int)&bss_size);

    if((int)(&bss_start) + (int)(&bss_size) >= 256*1024){
        printf("!!! WARNING !!! BSS conflict with kernel load address!\n");
    }
}

#define MAXHEAP (2 << 20) /* 2MB */

static void heap_init(void)
{
    int heap_size;

    // this is an attempt to leave enough space
    // above .data for us to load a sizeable
    // program (ie, linux). it's not ideal.
    // shame that talloc cannot allocate from
    // the top downwards.

    heap_size = ram_size / 4;  /* not more than 25% of RAM */
    if(heap_size > MAXHEAP)    /* and not too much */
        heap_size = MAXHEAP;

    heap_base = ram_size - heap_size;
    ta_init((void*)heap_base, (void*)ram_size - STACK_SIZE - 1, heap_size > (200*1024) ? 2048 : 256, 16, 4);
}

void mem_init(void)
{
    int shift;
    int heap_size;
    char unit;

    heap_init();

    if(ram_size >= 4*1024*1024){
        shift = 20;
        unit = 'M';
        // don't account for the stack here; we're rounding up
        heap_size = (ram_size - heap_base) >> shift;
    }else{
        shift = 10;
        unit = 'K';
        heap_size = (ram_size - heap_base - STACK_SIZE) >> shift;
    }

    printf("RAM installed: %ld %cB, %d %cB heap at 0x%08lx\n", 
            ram_size>>shift, unit, heap_size, unit, heap_base);
}

void gogoboot(void)
{

    early_init();

    uart_init();

    puts(copyright_msg);
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
