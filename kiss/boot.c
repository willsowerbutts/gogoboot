/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <stdbool.h>
#include <tinyalloc.h>
#include <disk.h>
#include <uart.h>
#include <net.h>
#include <cli.h>
#include <version.h>

extern const char copyright_msg[];

extern char text_start, text_size;
extern char stack_start, stack_size;
extern char rodata_start, rodata_size;
extern char data_start, data_load_start, data_size;
extern char bss_start, bss_size;

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

#define MAXHEAP (4 << 20) /* 4MB */

// static unsigned int heap_init(void)
// {
//     int heap;
//     void *base;
// 
//     // this is an attempt to leave enough space
//     // above .data for us to load a sizeable
//     // program (ie, linux). it's not ideal.
//     // shame that talloc cannot allocate from
//     // the top downwards.
// 
//     heap = ram_size / 4;  /* not more than 25% of RAM */
//     if(heap > MAXHEAP)    /* and not too much */
//         heap = MAXHEAP;
// 
//     base = (void*)ram_size - heap;
//     ta_init(base, (void*)ram_size-1, 2048, 16, 8);
// 
//     return (unsigned int)base;
// }

int some_global_variable = 0x12345678;

void boot_kiss(void)
{
    uart_init();

    printf(copyright_msg);
    report_linker_layout();

    printf("Version %s\n", software_version_string);

    printf("debug %x\n", some_global_variable);

    // printf("RAM installed: ");
    // q40_measure_ram_size();
    // unsigned int heap_base = heap_init();
    // printf("%d MB, %d MB heap at 0x%08x\n", ram_size>>20, (ram_size-heap_base)>>20, heap_base);

    // printf("Setup interrupts: ");
    // q40_setup_interrupts(); /* do this early to get timers ticking */
    // printf("done\n");

    // printf("Initialise RTC: ");
    // q40_rtc_init();

    // printf("\nInitialise video: ");
    // q40_graphics_init(3);
    // printf("done\n");

    // gogoboot_disk_init();

    // printf("Initialise ethernet: ");
    // net_init();
    // if(eth_init()){
    //     dhcp_init();
    // }

    // q40_led(true);

    command_line_interpreter();

    // q40_led(false);
    puts("[halted]");
}
