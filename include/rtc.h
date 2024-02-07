#ifndef __GOGOBOOT_RTC_DOT_H__
#define __GOGOBOOT_RTC_DOT_H__

typedef struct {
    uint16_t year;   /* 1980 -- 2079 */
    uint8_t month;   /* 1 -- 12 */
    uint8_t day;     /* 1 -- 31 */
    uint8_t hour;    /* 0 -- 23 */
    uint8_t minute;  /* 0 -- 59 */
    uint8_t second;  /* 0 -- 59 */
} rtc_time_t;

void rtc_read_clock(rtc_time_t *buffer);
void rtc_init(void);
void report_current_time(void);

#endif
