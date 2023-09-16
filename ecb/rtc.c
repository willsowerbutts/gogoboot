/* 2023-08-20 William R Sowerbutts */

#include <stdlib.h>
#include <types.h>
#include <fatfs/ff.h>
#include <rtc.h>
#include <ecb/ecb.h>

static uint8_t mfpic_rtc_shadow;

static void rtc_set_chipselect(bool enable)
{
    if(!enable)
        mfpic_rtc_shadow |= MFPIC_DS1302_RESET_BIT;
    else
        mfpic_rtc_shadow &= ~MFPIC_DS1302_RESET_BIT;
    ecb_write_byte_pause(MFPIC_RTC, mfpic_rtc_shadow);
}

static void rtc_set_clk(bool clk_high)
{
    if(clk_high)
        mfpic_rtc_shadow |= MFPIC_DS1302_CLK_BIT;
    else
        mfpic_rtc_shadow &= ~MFPIC_DS1302_CLK_BIT;
    ecb_write_byte_pause(MFPIC_RTC, mfpic_rtc_shadow);
}

static void rtc_set_data_direction(bool write)
{
    if(!write)
        mfpic_rtc_shadow &= ~MFPIC_DS1302_WREN_BIT;
    else
        mfpic_rtc_shadow |= MFPIC_DS1302_WREN_BIT;
    ecb_write_byte_pause(MFPIC_RTC, mfpic_rtc_shadow);
}

static void rtc_set_mosi(bool mosi_high)
{
    rtc_set_data_direction(true);
    if(mosi_high)
        mfpic_rtc_shadow |= MFPIC_DS1302_DATA_BIT;
    else
        mfpic_rtc_shadow &= ~MFPIC_DS1302_DATA_BIT;
    ecb_write_byte_pause(MFPIC_RTC, mfpic_rtc_shadow);
}

static bool rtc_get_miso(void)
{
    rtc_set_data_direction(false);
    return ecb_read_byte(MFPIC_RTC) & MFPIC_DS1302_DATA_BIT;
}

/* called with clk low, chip select enabled. clocks out 8 bit value. returns with clk low */
static void rtc_send_byte(uint8_t value)
{
    for(int bit=0; bit<8; bit++){
        rtc_set_mosi(value & 1);
        rtc_set_clk(true);
        value >>= 1;
        rtc_set_clk(false);
    }
}

static uint8_t rtc_read_byte(void)
{
    uint8_t value = 0;
    for(int bit=0; bit<8; bit++){
        value >>= 1;
        if(rtc_get_miso())
            value |= 0x80;
        rtc_set_clk(true);
        rtc_set_clk(false);
    }
    return value;
}

static void rtc_idle(void)
{
    rtc_set_clk(false);
    rtc_set_chipselect(false);
    rtc_set_data_direction(false);
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
    /* clock burst read */
    rtc_set_clk(false); /* clk must be driven low before rising edge of chip select line */
    rtc_set_chipselect(true);
    rtc_send_byte(0xBF); /* clk burst read */
    buffer->second  = decode_bcd_byte(rtc_read_byte() & 0x7f);
    buffer->minute  = decode_bcd_byte(rtc_read_byte());
    buffer->hour    = rtc_read_byte();
    if(buffer->hour & 0x80){ /* need to convert from 12 hour mode? */
        buffer->hour &= 0x7f;
        if(buffer->hour & 0x20){
            buffer->hour &= 0xdf;
            buffer->hour += 0x12;
            if((buffer->hour & 0x0f) >= 10) /* fix up carry */
                buffer->hour += 6;
        }
    }
    buffer->hour    = decode_bcd_byte(buffer->hour);
    buffer->day     = decode_bcd_byte(rtc_read_byte());
    buffer->month   = decode_bcd_byte(rtc_read_byte());
    rtc_read_byte(); /* weekday -- not used -- discard */
    buffer->year    = decode_bcd_byte(rtc_read_byte());
    buffer->year += (buffer->year >= 80) ? 1900 : 2000;
    rtc_idle(); /* all done */
}

void rtc_init(void)
{
    mfpic_rtc_shadow = 0;
    rtc_idle();
}
