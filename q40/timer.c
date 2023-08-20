/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <types.h>
#include <timers.h>
#include <q40/hw.h>

volatile uint32_t timer_ticks;

timer_t gogoboot_read_timer(void)
{
    return timer_ticks; /* it's a single aligned long word, so this should be atomic? */
}

void q40_setup_interrupts(void)
{
    /* configure MASTER chip */
    *q40_keyboard_interrupt_enable = 0;
    *q40_isa_interrupt_enable = 0;
    *q40_sample_interrupt_enable = 0;
    *q40_sample_rate = 0;
#if TIMER_HZ == 200
    *q40_frame_rate = 1;
#elif TIMER_HZ == 50
    *q40_frame_rate = 0;
#else
    #error Unsupported TIMER_HZ value (use 50 or 200)
#endif
    *q40_keyboard_interrupt_ack = 0xff;
    *q40_frame_interrupt_ack = 0xff;
    *q40_sample_interrupt_ack = 0xff;
    cpu_interrupts_on();
}
