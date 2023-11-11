#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"

#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "LCD_GUI.h"
#include "LCD_Touch.h"

#include "rtc.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

void set_rtc(const char *http_response) {
    const char *datetime_tag = "Date: ";
    const char *end_tag = " GMT";
    const char *datetime_ptr = strstr(http_response, datetime_tag);
    assert(datetime_ptr);
    datetime_ptr += strlen(datetime_tag);
    const char *end_ptr = strstr(http_response, end_tag);
    assert(end_ptr);
    size_t n = end_ptr - datetime_ptr;

    char datetime_str[40];
    memset(datetime_str, '\0', 40);
    strncpy(datetime_str, datetime_ptr, n);

    // Set RTC
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    strptime(datetime_str, "%a, %e %b %Y %T", &tm);
    // Manual timezone fix, since doing this with plain libc is bloody
    // frustrating
    tm.tm_hour++;
    mktime(&tm); // Should fix up the time into valid values

    datetime_t t = {.year = tm.tm_year,
                    .month = tm.tm_mon,
                    .day = tm.tm_mday,
                    .dotw = tm.tm_wday,
                    .hour = tm.tm_hour,
                    .min = tm.tm_min,
                    .sec = tm.tm_sec};
    rtc_set_datetime(&t);
    sleep_us(64); // sleep to ensure that the next rtc_get_dattime is getting
                  // the latest data
}

void render_time(void) {
    datetime_t t;
    rtc_get_datetime(&t);
    char datetime_str[40];
    sprintf(datetime_str, "%d:%02d:%02d, %d/%d, %d", t.hour, t.min, t.sec,
            t.day, t.month, t.year + 1900);

    // We first need to reset the LCD in the changed region
    GUI_DrawRectangle(20, 50, 480, 50 + 24, WHITE, DRAW_FULL, DOT_PIXEL_DFT);
    GUI_DisString_EN(20, 50, datetime_str, &Font24, LCD_BACKGROUND, BLACK);
}
