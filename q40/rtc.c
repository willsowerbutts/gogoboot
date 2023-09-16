/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <types.h>
#include <q40/isa.h>
#include <q40/hw.h>
#include <rtc.h>
#include <cpu.h>

void rtc_init(void)
{
    /* nothing to do on Q40 */
}

uint8_t q40_rtc_read_nvram(int offset)
{
    if(offset >= 0 && offset < Q40_RTC_NVRAM_SIZE)
        return *Q40_RTC_NVRAM(offset);
    return 0xff;
}

void q40_rtc_write_nvram(int offset, uint8_t value)
{
    if(offset >= 0 && offset < Q40_RTC_NVRAM_SIZE)
        *Q40_RTC_NVRAM(offset) = value;
}

static uint8_t q40_rtc_read_control(void)
{
    return *Q40_RTC_REGISTER(0);
}

static void q40_rtc_write_control(uint8_t value)
{
    *Q40_RTC_REGISTER(0) = value;
}

static int decode_bcd_digit(uint8_t val)
{
    val &= 0x0f;
    return (val > 9) ? 0 : val;
}

static int decode_bcd_byte(uint8_t val)
{
    return (decode_bcd_digit(val>>4)*10) + decode_bcd_digit(val);
}

void rtc_read_clock(rtc_time_t *buffer)
{
    uint8_t ctrl = q40_rtc_read_control();
    q40_rtc_write_control(ctrl | 0x40); /* set READ bit */
    buffer->second  = decode_bcd_byte(*Q40_RTC_REGISTER(1) & 0x7f);
    buffer->minute  = decode_bcd_byte(*Q40_RTC_REGISTER(2) & 0x7f);
    buffer->hour    = decode_bcd_byte(*Q40_RTC_REGISTER(3) & 0x3f);
    /* do not read weekday at offset 4 -- not used */
    buffer->day     = decode_bcd_byte(*Q40_RTC_REGISTER(5) & 0x3f);
    buffer->month   = decode_bcd_byte(*Q40_RTC_REGISTER(6) & 0x1f);
    buffer->year    = decode_bcd_byte(*Q40_RTC_REGISTER(7));
    buffer->year   += (buffer->year >= 80) ? 1900 : 2000;
    q40_rtc_write_control(ctrl & ~0xC0); /* unset READ, WRITE bits */
}

// void q40_rtc_write_clock(const q40_rtc_data_t *buffer)
// {
//     uint8_t ctrl = q40_rtc_read_control();
//     q40_rtc_write_control(ctrl | 0x80); /* set WRITE bit */
//     *Q40_RTC_REGISTER(1) = buffer->second;
//     *Q40_RTC_REGISTER(2) = buffer->minute;
//     *Q40_RTC_REGISTER(3) = buffer->hour;
//     *Q40_RTC_REGISTER(4) = buffer->weekday;
//     *Q40_RTC_REGISTER(5) = buffer->day;
//     *Q40_RTC_REGISTER(6) = buffer->month;
//     *Q40_RTC_REGISTER(7) = buffer->year;
//     q40_rtc_write_control(ctrl & ~0xC0); /* unset READ, WRITE bits */
// }
