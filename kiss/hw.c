/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <kiss/hw.h>

uint32_t ram_size = 0;

#define RAM_UNIT_SIZE (1024*1024)       /* smallest granularity */

void measure_ram_size(void)
{
    /* we write a longword at the end of each MB of RAM, from
       the highest possible address downwards. Then we read
       these back, in the reverse order, to confirm how much
       RAM is actually fitted. Because the lowest address we
       write to is just below 1MB we avoid stomping on any of
       our code, data or stack which is all far below 1MB. 

       On KISS-68030 we need to be careful not to cause a bus
       exception when the DOUBLE jumper is set to 0
    */

    int max_ram_possible = kiss_check_double_jumper(); /* in MB */

    #define UNIT_ADDRESS(unit) ((uint32_t*)(unit * RAM_UNIT_SIZE - sizeof(uint32_t)))
    #define UNIT_TEST_VALUE(unit) ((uint32_t)(unit | ((~unit) << 16)))

    ram_size = 0;

    for(int unit=max_ram_possible; unit > 0; unit--)
        *UNIT_ADDRESS(unit) = UNIT_TEST_VALUE(unit);

    for(int unit=1; unit<=max_ram_possible; unit++)
        if(*UNIT_ADDRESS(unit) == UNIT_TEST_VALUE(unit))
            ram_size = (unit * RAM_UNIT_SIZE);
        else
            break;
}

void early_init(void)
{
    // not required
}

void target_hardware_init(void)
{
    // not required
}

void rtc_init(void)
{
    printf("no driver yet!\n");
}
