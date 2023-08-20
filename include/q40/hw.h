#ifndef __Q40HW_DOT_H__
#define __Q40HW_DOT_H__

#include <stdbool.h>
#include <types.h>
#include <timers.h>

void q40_graphics_init(int mode);
void q40_isa_reset(void);
void q40_led(bool on);
void q40_measure_ram_size(void); /* sets ram_size; overwrites some areas of memory */
void q40_setup_interrupts(void);
void q40_boot_softrom(void *rom_image);
void q40_boot_qdos(void *qdos_image);

/* in startup.s */
void cpu_cache_disable(void);
void cpu_cache_flush(void);
void cpu_cache_invalidate(void);
void cpu_interrupts_on(void);
void cpu_interrupts_off(void);

/* RTC */
#define Q40_RTC_NVRAM_SIZE (2040) /* bytes */
#define Q40_RTC_CLOCK_SIZE (8)    /* bytes */
typedef struct {
    /* see datasheet -- these are BCD encoded, with some other bits stuffed in there */
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} q40_rtc_data_t;

void q40_rtc_init(void);
uint8_t q40_rtc_read_nvram(int offset);
void    q40_rtc_write_nvram(int offset, uint8_t value);
uint8_t q40_rtc_read_control(void);
void q40_rtc_write_control(uint8_t value);
void q40_rtc_read_clock(q40_rtc_data_t *buffer);
void q40_rtc_write_clock(const q40_rtc_data_t *buffer);

extern unsigned int ram_size; /* use only after q40_measure_ram_size() called */
extern void *rom_pointer;     /* this is a pointer to 0 that the compiler doesn't know will always point to 0 */

/* hardware details */
#define VIDEO_RAM_BASE  0xfe800000
#define MASTER_ADDRESS  0xff000000
#define RTC_ADDRESS     0xff020000

#define MAX_RAM_SIZE  32                /* in MB; code needs adjusting to support 128MB option boards */
#define RAM_UNIT_SIZE (1024*1024)       /* smallest granularity */
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

#endif
