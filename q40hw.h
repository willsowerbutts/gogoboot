#ifndef __Q40HW_DOT_H__
#define __Q40HW_DOT_H__

#include <stdbool.h>

extern unsigned int ram_size;

void q40_delay(uint32_t count);
void q40_graphics_init(int mode);
void q40_isa_reset(void);
void q40_led(bool on);
void q40_measure_ram_size(void); /* sets ram_size */
void q40_setup_interrupts(void);

/* in startup.s */
void cpu_cache_disable(void);
void cpu_cache_flush(void);
void cpu_set_ipl(int ipl);

#define VIDEO_RAM_BASE  0xfe800000
#define MASTER_ADDRESS  0xff000000

#define MAX_RAM_SIZE  128 /* MB */
#define RAM_UNIT_SIZE (1024*1024) /* smallest granularity */

#define Q40_REGISTER(offset, name) \
    static volatile uint8_t * const (name) = (volatile uint8_t *)(MASTER_ADDRESS + (offset))

Q40_REGISTER(0x00, q40_interrupt_status);               /* read-only */
Q40_REGISTER(0x04, q40_isa_interrupt_status);           /* read-only */
Q40_REGISTER(0x08, q40_keyboard_interrupt_enable);      /* write-only (bit 0 = enable) */
Q40_REGISTER(0x10, q40_isa_interrupt_enable);           /* write-only (bit 0 = enable) */
Q40_REGISTER(0x14, q40_sample_interrupt_enable);        /* write-only (bit 0 = enable) */
Q40_REGISTER(0x18, q40_display_control);                /* write-only (bits 0,1 = mode) */
Q40_REGISTER(0x1C, q40_keycode_register);               /* read-only */
Q40_REGISTER(0x20, q40_keyboard_interrupt_ack);         /* write-only */
Q40_REGISTER(0x24, q40_frame_interrupt_ack);            /* write-only */
Q40_REGISTER(0x28, q40_sample_interrupt_ack);           /* write-only */
Q40_REGISTER(0x2C, q40_sample_rate);                    /* write-only */
Q40_REGISTER(0x30, q40_led_control);                    /* write-only */
Q40_REGISTER(0x34, q40_isa_bus_reset);                  /* write-only */
Q40_REGISTER(0x38, q40_frame_rate);                     /* write-only */

#endif
