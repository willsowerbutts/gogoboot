/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>

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
