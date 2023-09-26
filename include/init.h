#ifndef __INIT_DOT_H__
#define __INIT_DOT_H__

#include <stdbool.h>

#define DEFAULT_STACK_SIZE 8192
#define MAXHEAP (2 << 20) /* 2MB */

/* copyright/startup message from early ROM */
extern const char copyright_msg[];

extern uint32_t ram_size;
extern uint32_t stack_base, stack_size, stack_top;
extern uint32_t heap_base, heap_size;
extern uint32_t bounce_below_addr, rom_below_addr;

void early_init(void);
void target_hardware_init(void);
void setup_interrupts(void);
void measure_ram_size(void);
void report_memory_layout(void);
const char *check_writable_range(uint32_t base, uint32_t length, bool can_bounce);

/* target provides these, used by measure_ram_size */
/* these are called with a relatively small stack! */
uint32_t mem_get_max_possible(void);
uint32_t mem_get_granularity(void);
void target_mem_init(void);

/* linker provides these symbols */
extern const char text_start, text_size;
extern const char rodata_start, rodata_size;
extern const char data_start, data_load_start, data_size;
extern const char bss_start, bss_size, bss_end;

#endif
