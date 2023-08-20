/* 2023-08-20 William R Sowerbutts */

#include <stdlib.h>
#include <types.h>
#include <tinyalloc.h>
#include <fatfs/ff.h>
#include <fatfs/diskio.h>
#include <disk.h>
#include <q40/hw.h>

/* glue for FatFs library */

static int decode_bcd_digit(uint8_t val)
{
    val &= 0x0f;
    return (val > 9) ? 0 : val;
}

static int decode_bcd_byte(uint8_t val)
{
    return (decode_bcd_digit(val>>4)*10) + decode_bcd_digit(val);
}

DWORD get_fattime (void)
{
    uint32_t result;
    /*
        bit31:25 Year origin from the 1980 (0..127, e.g. 37 for 2017)
        bit24:21 Month (1..12)
        bit20:16 Day of the month (1..31)
        bit15:11 Hour (0..23)
        bit10:5  Minute (0..59)
        bit4:0   Second / 2 (0..29, e.g. 25 for 50)
    */
    q40_rtc_data_t now;
    q40_rtc_read_clock(&now);
    result = decode_bcd_byte(now.year);
    if(result <= 80)
        result += 20;
    result  = result                              << 25;
    result |= decode_bcd_byte(now.month   & 0x1F) << 21;
    result |= decode_bcd_byte(now.day     & 0x3F) << 16;
    result |= decode_bcd_byte(now.hour    & 0x3F) << 11;  
    result |= decode_bcd_byte(now.minute  & 0x7F) <<  5;
    result |= decode_bcd_byte(now.second  & 0x7F) >>  1;
    return result;
}
