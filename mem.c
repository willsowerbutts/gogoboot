/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <init.h>

uint32_t ram_size, stack_base, heap_base;

void measure_ram_size(void)
{
    /* 
       We write a longword at the end of each unit (typically 
       1MB) of RAM, from the highest possible address downwards. 
       Then we read these back and check them, in the reverse 
       order, to determine how much RAM is actually fitted. 

       Take care to ensure you don't stomp on your code/data.

       This is called with a relatively small (256-byte) stack
    */

    uint32_t max_ram = mem_get_max_possible();
    uint32_t unit_size = mem_get_granularity();
    uint32_t max_units = max_ram / unit_size;
    ram_size = 0;

    #define UNIT_ADDRESS(unit) ((uint32_t*)(unit * unit_size - sizeof(uint32_t)))
    #define UNIT_TEST_VALUE(unit) ((uint32_t)(unit | ((~unit) << 16)))

    for(int unit=max_units; unit > 0; unit--)
        *UNIT_ADDRESS(unit) = UNIT_TEST_VALUE(unit);

    for(int unit=1; unit<=max_units; unit++)
        if(*UNIT_ADDRESS(unit) == UNIT_TEST_VALUE(unit))
            ram_size = (unit * unit_size);
        else
            break;

    stack_base = ram_size - STACK_SIZE;
}
