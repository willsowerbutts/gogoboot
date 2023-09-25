/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <kiss/hw.h>

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
