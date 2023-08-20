#ifndef __TIMERS_DOT_H__
#define __TIMERS_DOT_H__

#include <stdbool.h>
#include <types.h>

#define TIMER_HZ                200     // choose 50 or 200
#define TIMER_MS_PER_TICK       (1000/TIMER_HZ)

/* timers - on Q40, use only after q40_setup_interrupts() called */
typedef uint32_t timer_t;
timer_t gogoboot_read_timer(void);
timer_t set_timer_ticks(uint32_t duration); /* duration in ticks */
bool timer_expired(timer_t timer);
void timer_wait(timer_t timeout);
#define set_timer_ms(msec) set_timer_ticks(((msec)+TIMER_MS_PER_TICK-1)/TIMER_MS_PER_TICK)
#define set_timer_sec(sec) set_timer_ms((sec)*1000)

/* timer based delays */
#define delay_sec(sec) timer_wait(set_timer_sec((sec)))
#define delay_ms(msec) timer_wait(set_timer_ms((msec)))

#endif
