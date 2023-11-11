#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"

#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "LCD_GUI.h"
#include "LCD_Touch.h"

#include "main.h"
#include "network.h"
#include "tiny-json.h"
#include "tram.h"

#include <string.h>

void render_tram(const char *http_response) {
    const char *json_start = strchr(http_response, '{'); // First occurence of {
    assert(json_start != NULL);
    const char *json_end = strrchr(http_response, '}'); // Last occurence of }
    assert(json_end != NULL);

    // Create null-terminated string with just JSON
    char json_response_str[HTTPS_RESPONSE_MAX_SIZE];
    memset(json_response_str, '\0', sizeof(json_response_str));
    memcpy(json_response_str, json_start, json_end - json_start + 1);

    json_t pool[MAX_JSON_FIELDS];
    const json_t *json = json_create(json_response_str, pool, MAX_JSON_FIELDS);
    assert(json);

    GUI_DrawRectangle(20, 140, 480, 170 + 24, WHITE, DRAW_FULL, DOT_PIXEL_DFT);
    const json_t *departures_field = json_getProperty(json, "departures");
    assert(json_getType(departures_field) == JSON_ARRAY);

    struct tm tram14[10];
    struct tm tram24[10];
    memset(tram14, 0, sizeof(tram14));
    memset(tram24, 0, sizeof(tram24));
    size_t tram14_index = 0;
    size_t tram24_index = 0;

    datetime_t t;
    rtc_get_datetime(&t);
    struct tm current_tm = {.tm_year = t.year,
                            .tm_mon = t.month,
                            .tm_mday = t.day,
                            .tm_wday = t.dotw,
                            .tm_hour = t.hour,
                            .tm_min = t.min,
                            .tm_sec = t.sec};

    for (const json_t *departure_field = json_getChild(departures_field);
         departure_field != NULL;
         departure_field = json_getSibling(departure_field)) {
        assert(json_getType(departure_field) == JSON_OBJ);
        const json_t *route_field = json_getProperty(departure_field, "route");
        assert(json_getType(route_field) == JSON_OBJ);
        const json_t *short_name_field =
            json_getProperty(route_field, "short_name");
        assert(json_getType(short_name_field) == JSON_TEXT);
        const char *short_name = json_getValue(short_name_field);
        const json_t *arrival_field =
            json_getProperty(departure_field, "arrival_timestamp");
        assert(json_getType(arrival_field) == JSON_OBJ);
        const json_t *predicted_field =
            json_getProperty(arrival_field, "predicted");
        assert(json_getType(predicted_field) == JSON_TEXT);
        char *predicted = (char *)json_getValue(predicted_field);

        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        for (size_t i = 0; i < strlen(predicted); ++i)
            if (predicted[i] == 'T')
                predicted[i] = ' ';
        strptime(predicted, "%Y-%m-%d %T+01:00", &tm);
        struct tm diff_tm = {
            .tm_year = tm.tm_year - current_tm.tm_year,
            .tm_mon = tm.tm_mon - current_tm.tm_mon,
            .tm_mday = tm.tm_mday - current_tm.tm_mday,
            .tm_wday = tm.tm_wday - current_tm.tm_wday,
            .tm_hour = tm.tm_hour - current_tm.tm_hour,
            .tm_min = tm.tm_min - current_tm.tm_min,
            .tm_sec = tm.tm_sec - current_tm.tm_sec,
        };
        mktime(&diff_tm);

        if (strcmp(short_name, "14") == 0) {
            tram14[tram14_index++] = diff_tm;
        }
        if (strcmp(short_name, "24") == 0) {
            tram24[tram24_index++] = diff_tm;
        }
    }

    {
        char outp_str[40] = "14: ";
        size_t ind = 4;
        for (size_t i = 0; i < tram14_index; ++i) {
            if (i != 0)
                ind += sprintf(outp_str + ind, ", ");
            if (tram14[i].tm_min != 0)
                ind += sprintf(outp_str + ind, "%dm", tram14[i].tm_min);
            ind += sprintf(outp_str + ind, "%ds", tram14[i].tm_sec);
        }
        GUI_DisString_EN(20, 140, outp_str, &Font24, LCD_BACKGROUND, BLACK);
    }
    {
        char outp_str[40] = "24: ";
        size_t ind = 4;
        for (size_t i = 0; i < tram24_index; ++i) {
            if (i != 0)
                ind += sprintf(outp_str + ind, ", ");
            if (tram24[i].tm_min != 0)
                ind += sprintf(outp_str + ind, "%dm", tram24[i].tm_min);
            ind += sprintf(outp_str + ind, "%ds", tram24[i].tm_sec);
        }
        GUI_DisString_EN(20, 170, outp_str, &Font24, LCD_BACKGROUND, BLACK);
    }
}
