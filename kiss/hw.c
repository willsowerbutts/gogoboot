/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <init.h>
#include <kiss/hw.h>
#include <ecb/ecb.h>

uint32_t mem_get_max_possible(void)
{
    /* On KISS-68030 we need to be careful not to cause a bus
       exception when the DOUBLE jumper is set to 0 */
    return (kiss_check_double_jumper() ? 256 : 64) << 20;
}

uint32_t mem_get_granularity(void)
{
    return 1024*1024;
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
    stack_size = DEFAULT_STACK_SIZE;

    if((uint32_t)&data_start >= KISS68030_SRAM_BASE){
        /* 8-bit SRAM target */
        stack_top = KISS68030_SRAM_BASE + 0x8000;
        stack_base = stack_top - stack_size;
        bounce_below_addr = 0;
        /* small heap squeezes in between bss and stack */
        heap_base = (uint32_t)&bss_end;
        heap_size = stack_base - heap_base;
    }else{
        /* 32-bit DRAM target */
        stack_top = ram_size;
        stack_base = ram_size - stack_size;
        /* attempts to load_data() into addresses below bounce_below_addr 
         * will result in the bounce buffer being employed */
        bounce_below_addr = (((uint32_t)&bss_end) + 3) & ~3; /* round to longword */

        heap_size = ram_size / 4;  /* not more than 25% of RAM */
        if(heap_size > MAXHEAP)    /* and not too much */
            heap_size = MAXHEAP;

        heap_base = ram_size - heap_size;
        heap_size -= stack_size;
    }
}
