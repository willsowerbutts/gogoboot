#ifndef __INIT_DOT_H__
#define __INIT_DOT_H__

#define DEFAULT_STACK_SIZE 8192

extern uint32_t ram_size, stack_base, stack_size, heap_base, heap_size;

void early_init(void);
void target_hardware_init(void);
void setup_interrupts(void);
void rtc_init(void);
void measure_ram_size(void);

/* target provides these, used by measure_ram_size */
uint32_t mem_get_max_possible(void);
uint32_t mem_get_granularity(void);

#endif
