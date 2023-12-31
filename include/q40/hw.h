#ifndef __Q40HW_DOT_H__
#define __Q40HW_DOT_H__

#include <stdbool.h>
#include <types.h>
#include <timers.h>

void q40_graphics_init(int mode);
void q40_isa_reset(void);
void q40_led(bool on);
void q40_boot_softrom(void *rom_image);
void q40_boot_qdos(void *qdos_image);

/* RTC */
#define Q40_RTC_NVRAM_SIZE (2040) /* bytes */
#define Q40_RTC_CLOCK_SIZE (8)    /* bytes */

uint8_t q40_rtc_read_nvram(int offset);
void    q40_rtc_write_nvram(int offset, uint8_t value);

/* hardware details */
#define VIDEO_RAM_BASE  0xfe800000
#define MASTER_ADDRESS  0xff000000
#define RTC_ADDRESS     0xff020000

#define MAX_RAM_SIZE  32                /* in MB; code needs adjusting to support 128MB option boards */
#define Q40_ROMSIZE   (96*1024)         /* size of low ROM alias at base of physical memory */

#define Q40_RTC_NVRAM(offset) ((volatile uint8_t *)(RTC_ADDRESS + (4 * offset)))
#define Q40_RTC_REGISTER(offset) ((volatile uint8_t *)(RTC_ADDRESS + (4 * Q40_RTC_NVRAM_SIZE) + (4 * offset)))

#define Q40_MASTER_REGISTER(offset, name) \
    static volatile uint8_t * const (name) = (volatile uint8_t *)(MASTER_ADDRESS + (offset))

Q40_MASTER_REGISTER(0x00, q40_interrupt_status);               /* read-only */
Q40_MASTER_REGISTER(0x04, q40_isa_interrupt_status);           /* read-only */
Q40_MASTER_REGISTER(0x08, q40_keyboard_interrupt_enable);      /* write-only (bit 0 = enable) */
Q40_MASTER_REGISTER(0x10, q40_isa_interrupt_enable);           /* write-only (bit 0 = enable) */
Q40_MASTER_REGISTER(0x14, q40_sample_interrupt_enable);        /* write-only (bit 0 = enable) */
Q40_MASTER_REGISTER(0x18, q40_display_control);                /* write-only (bits 0,1 = mode) */
Q40_MASTER_REGISTER(0x1C, q40_keycode_register);               /* read-only */
Q40_MASTER_REGISTER(0x20, q40_keyboard_interrupt_ack);         /* write-only */
Q40_MASTER_REGISTER(0x24, q40_frame_interrupt_ack);            /* write-only */
Q40_MASTER_REGISTER(0x28, q40_sample_interrupt_ack);           /* write-only */
Q40_MASTER_REGISTER(0x2C, q40_sample_rate);                    /* write-only */
Q40_MASTER_REGISTER(0x30, q40_led_control);                    /* write-only */
Q40_MASTER_REGISTER(0x34, q40_isa_bus_reset);                  /* write-only */
Q40_MASTER_REGISTER(0x38, q40_frame_rate);                     /* write-only */

/* this is a pointer to the ROM (at 0), but the compiler doesn't know it
 * does, and will always, point to 0 -- so the compiler doesn't try to make
 * certain "optimisations", like calling trap #7 when we access it ... */
extern void *rom_pointer;     

#endif
