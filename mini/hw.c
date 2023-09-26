/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <init.h>

uint32_t mem_get_max_possible(void)
{
    /* we test only the onboard SRAM */
    return 2*1024*1024;
}

uint32_t mem_get_granularity(void)
{
    return 512*1024;
}

uint32_t mem_get_rom_below_addr(void)
{
    return 0;
}

void early_init(void)
{
    // not required
}

void target_hardware_init(void)
{
    // not required
}

void target_mem_init(void)
{
    rom_below_addr = 0;
    stack_base = ram_size - DEFAULT_STACK_SIZE;
    stack_size = DEFAULT_STACK_SIZE;
    stack_top = stack_base + stack_size;
    /* attempts to load_data() into addresses below bounce_below_addr 
     * will result in the bounce buffer being employed */
    bounce_below_addr = (((uint32_t)&bss_end) + 3) & ~3; /* round to longword */

    heap_size = ram_size / 4;  /* not more than 25% of RAM */
    if(heap_size > MAXHEAP)    /* and not too much */
        heap_size = MAXHEAP;

    heap_base = ram_size - heap_size;
    heap_size -= stack_size;
}
