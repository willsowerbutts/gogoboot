/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <types.h>
#include <timers.h>

timer_t set_timer_ticks(uint32_t duration_ticks)
{
    if(duration_ticks == 0){ 
        /* round up very short durations */
        duration_ticks = 1;
    }else if(duration_ticks & 0x80000000){
        /* limit very long durations (avoid wraparound issues) */
        printf("bad timer duration %ld\n", duration_ticks);
        duration_ticks = 0x7fffffff;
    }
    return gogoboot_read_timer() + duration_ticks;
}

bool timer_expired(timer_t timer)
{
    return ((timer - gogoboot_read_timer()) & 0x80000000);
}

void timer_wait(timer_t timeout)
{
    while(!timer_expired(timeout));
}
