/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <init.h>

uint32_t ram_size;
uint32_t stack_base, stack_size, stack_top;
uint32_t heap_base, heap_size;
uint32_t bounce_below_addr, rom_below_addr;
extern const char bss_end; /* linker provides this symbol */

/* TODO: have a list of RAM regions?
 * EG Q40/Q60 with 128MB option have non-contiguous DRAM, and separate VRAM region
 * There's no reason RAM must be in one big chunk.
 */

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
    rom_below_addr = mem_get_rom_below_addr();
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

    stack_base = ram_size - DEFAULT_STACK_SIZE;
    stack_size = DEFAULT_STACK_SIZE;
    stack_top = stack_base + stack_size;

    /* attempts to load_data() into addresses below bounce_below_addr 
     * will result in the bounce buffer being employed */
    bounce_below_addr = (((uint32_t)&bss_end) + 3) & ~3; /* round to longword */
}

const char *check_writable_range(uint32_t base, uint32_t length, bool can_bounce)
{
    if(base + length > ram_size)
        return "past end of RAM";
    if(base + length > heap_base)
        return "overlaps heap memory";
    if(base < rom_below_addr)
        return "overlaps ROM";
    if(!can_bounce && base < bounce_below_addr)
        return "overlaps gogoboot memory";
    /* if you get here, no problem! */
    return NULL;
}

