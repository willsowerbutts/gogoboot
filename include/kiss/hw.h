#ifndef __KISSHW_DOT_H__
#define __KISSHW_DOT_H__

#include <stdbool.h>
#include <types.h>
#include <timers.h>

extern uint32_t ram_size;

void halt(void);
void kiss_setup_interrupts(void);
void kiss_measure_ram_size(void);
uint32_t kiss_check_double_jumper(void);

#endif
