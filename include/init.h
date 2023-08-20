#ifndef __INIT_DOT_H__
#define __INIT_DOT_H__

void early_init(void);
void late_init(void);
void target_hardware_init(void);
void setup_interrupts(void);
void rtc_init(void);
void measure_ram_size(void);
void halt(void);

extern uint32_t ram_size;     /* use only after measure_ram_size() called */
extern void *rom_pointer;     /* this is a pointer to 0 that the compiler doesn't know will always point to 0 */

#endif
