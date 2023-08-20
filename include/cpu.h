#ifndef __CPU_DOT_H__
#define __CPU_DOT_H__

/* in startup.s */
void cpu_cache_disable(void);
void cpu_cache_flush(void);
void cpu_cache_invalidate(void);
void cpu_interrupts_on(void);
void cpu_interrupts_off(void);

#endif
