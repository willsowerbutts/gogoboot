#ifndef __Q40HW_DOT_H__
#define __Q40HW_DOT_H__

#include <stdbool.h>

extern unsigned int ram_size;

void q40_led(bool on);
void q40_measure_ram_size(void); /* sets ram_size */
void q40_graphics_init(int mode);
void q40_delay(uint32_t count);
void cpu_cache_disable(void); /* in startup.s */

#endif
