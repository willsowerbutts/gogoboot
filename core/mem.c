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

    typedef enum { mem_ram, mem_video, mem_rom } mem_type_t;
    typedef struct mem_t mem_t;
    
    struct memt_t {
        uint32_t base;
        uint32_t size;
        uint32_t first_free; // offset from base
        uint32_t last_free;  // offset from base
        mem_type_t type;
    };

    // ...
    add_memory(0, ram_size, mem_ram, &bss_end, heap_base);

    // first_free, last_free: so check_writable_range() can avoid data/bss/heap/stack in RAM
    // or maybe have different mem_ram_free/mem_ram_used ranges?
    // or have one type per segment ie mem_text, mem_rodata, mem_data, mem_bss, mem_heap, mem_stack etc?
    // could eliminate the need for rom_below_addr, bounce_below_addr; check_writable_range might be just a switch statement.
 *
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

    target_mem_init();
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

