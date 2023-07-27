#include <stdlib.h>
#include <q40types.h>
#include "q40isa.h"
#include "q40hw.h"

unsigned int ram_size = 0;

void q40_setup_interrupts(void)
{
    /* configure MASTER chip */
    *q40_keyboard_interrupt_enable = 0;
    *q40_isa_interrupt_enable = 0;
    *q40_sample_interrupt_enable = 0;
    *q40_keyboard_interrupt_ack = 0xff;
    *q40_frame_interrupt_ack = 0xff;
    *q40_sample_interrupt_ack = 0xff;
    *q40_sample_rate = 0;
    *q40_frame_rate = 1; /* 200Hz timer interrupts, please! */
    cpu_set_ipl(1);      /* enable interrupt 2 and above */
}

void q40_isa_reset(void)
{
    *q40_isa_bus_reset = 0xff;
    q40_delay(400000);
    *q40_isa_bus_reset = 0;
}

void q40_delay(uint32_t count)
{
    uint32_t x;
    while(count--)
        x += *q40_interrupt_status;
}

void q40_led(bool on)
{
    *q40_led_control = on ? 0xff : 0;
}

void q40_graphics_init(int mode)
{
    // behold my field of modes, in it there grow but four
    mode &= 3;

    // program the display controller
    *q40_display_control = mode;

    // clear entire video memory (1MB)
    memset((void*)VIDEO_RAM_BASE, 0, 1024*1024);
}

void q40_measure_ram_size(void)
{
    /* we write a longword at the end of each MB of RAM, from
       the highest possible address downwards. Then we read
       these back, in the reverse order, to confirm how much
       RAM is actually fitted. Because the lowest address we
       write to is just below 1MB we avoid stomping on any of
       our code, data or stack which is all far below 1MB. */

    #define UNIT_ADDRESS(unit) ((uint32_t*)(unit * RAM_UNIT_SIZE - sizeof(uint32_t)))
    #define UNIT_TEST_VALUE(unit) ((uint32_t)(unit | ((~unit) << 16)))

    ram_size = 0;

    for(int unit=MAX_RAM_SIZE; unit > 0; unit--)
        *UNIT_ADDRESS(unit) = UNIT_TEST_VALUE(unit);

    for(int unit=1; unit<=MAX_RAM_SIZE; unit++)
        if(*UNIT_ADDRESS(unit) == UNIT_TEST_VALUE(unit))
            ram_size = (unit * RAM_UNIT_SIZE);
        else
            break;
}
