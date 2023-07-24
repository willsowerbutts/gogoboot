#include <stdlib.h>
#include "q40hw.h"

#define VIDEO_RAM_BASE  0xfe800000
#define MASTER_ADDRESS  0xff000000
#define MASTER_LED      0x30
#define MASTER_DISPLAY  0x18

#define Q40_REGISTER(offset, name) \
    static volatile unsigned char * const (name) = (volatile unsigned char *)(MASTER_ADDRESS + (offset))

Q40_REGISTER(MASTER_LED,     q40_master_led);
Q40_REGISTER(MASTER_DISPLAY, q40_master_display);

void q40_led(bool on)
{
    *q40_master_led = on ? 0xff : 0;
}

void q40_graphics_init(int mode)
{
    // we only have four modes
    mode &= 3;

    // program the display controller
    *q40_master_display = mode;

    // clear entire video memory (1MB)
    memset((void*)VIDEO_RAM_BASE, 0, 1024*1024);
}

#define MAX_RAM_SIZE  128 /* MB */
#define RAM_UNIT_SIZE (1024*1024)
#define UNIT_ADDRESS(unit) ((uint32_t*)(unit * RAM_UNIT_SIZE - sizeof(uint32_t)))
#define UNIT_TEST_VALUE(unit) ((uint32_t)(unit | ((~unit) << 16)))

unsigned int ram_size = 0;

void q40_measure_ram_size(void)
{
    /* we write a longword at the end of each MB of RAM, from
       the highest possible address downwards. Then we read
       these back, in the reverse order, to confirm how much
       RAM is actually fitted. Because the lowest address we
       write to is just below 1MB we avoid stomping on any of
       our code, data or stack which is all far below 1MB. */

    ram_size = 0;

    for(int unit=MAX_RAM_SIZE; unit > 0; unit--)
        *UNIT_ADDRESS(unit) = UNIT_TEST_VALUE(unit);

    for(int unit=1; unit<=MAX_RAM_SIZE; unit++)
        if(*UNIT_ADDRESS(unit) == UNIT_TEST_VALUE(unit))
            ram_size = (unit * RAM_UNIT_SIZE);
        else
            break;
}
