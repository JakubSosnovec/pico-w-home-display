#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "pico/util/datetime.h"

#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "LCD_GUI.h"
#include "LCD_Touch.h"

#include "network.h"
#include "tiny-json.h"
#include "tram.h"

#include <string.h>

#define MAX_TRAM_LINE_STRING_LENGTH 40

#define MAX_RECORDS_PER_LINE 4
struct tram_state {
    critical_section_t cs;
    struct tm tram14[MAX_RECORDS_PER_LINE];
    struct tm tram18[MAX_RECORDS_PER_LINE];
    struct tm tram24[MAX_RECORDS_PER_LINE];
};
static struct tram_state state;

static void fill_string_arrivals(char *str, size_t n, struct tm *current_tm,
                                 struct tm *trams) {
    size_t ind = 0;
    for (size_t i = 0; i < MAX_RECORDS_PER_LINE && trams[i].tm_year != 0; ++i) {
        struct tm diff_tm = {
            .tm_year = trams[i].tm_year - current_tm->tm_year,
            .tm_mon = trams[i].tm_mon - current_tm->tm_mon,
            .tm_mday = trams[i].tm_mday - current_tm->tm_mday,
            .tm_wday = trams[i].tm_wday - current_tm->tm_wday,
            .tm_hour = trams[i].tm_hour - current_tm->tm_hour,
            .tm_min = trams[i].tm_min - current_tm->tm_min,
            .tm_sec = trams[i].tm_sec - current_tm->tm_sec,
        };
        mktime(&diff_tm);

        // If the tram has already arrived, mktime will convert the negative
        // difference into large minutes
        if (diff_tm.tm_min >= 15)
            break;

        if (i != 0)
            ind += snprintf(str + ind, n - ind, ", ");
        if (diff_tm.tm_min != 0)
            ind += snprintf(str + ind, n - ind, "%dm", diff_tm.tm_min);
        ind += snprintf(str + ind, n - ind, "%ds", diff_tm.tm_sec);
    }
}

void init_tram(void) { critical_section_init(&state.cs); }

void update_tram(const char *http_response) {
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

    const json_t *departures_field = json_getProperty(json, "departures");
    assert(json_getType(departures_field) == JSON_ARRAY);

    size_t tram14_index = 0;
    size_t tram18_index = 0;
    size_t tram24_index = 0;

    critical_section_enter_blocking(&state.cs);
    memset(state.tram14, 0, sizeof(state.tram14));
    memset(state.tram18, 0, sizeof(state.tram18));
    memset(state.tram24, 0, sizeof(state.tram24));

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

        if (strcmp(short_name, "14") == 0) {
            state.tram14[tram14_index++] = tm;
        }
        if (strcmp(short_name, "18") == 0) {
            state.tram18[tram18_index++] = tm;
        }
        if (strcmp(short_name, "24") == 0) {
            state.tram24[tram24_index++] = tm;
        }
    }
    critical_section_exit(&state.cs);
}

void render_tram(void) {
    datetime_t t;
    rtc_get_datetime(&t);
    struct tm current_tm = {.tm_year = t.year,
                            .tm_mon = t.month,
                            .tm_mday = t.day,
                            .tm_wday = t.dotw,
                            .tm_hour = t.hour,
                            .tm_min = t.min,
                            .tm_sec = t.sec};

    critical_section_enter_blocking(&state.cs);
    GUI_DrawRectangle(20, 140, 480, 200 + 24, WHITE, DRAW_FULL, DOT_PIXEL_DFT);
    {
        char outp_str[MAX_TRAM_LINE_STRING_LENGTH] = "14: ";
        fill_string_arrivals(outp_str + 4, MAX_TRAM_LINE_STRING_LENGTH - 4,
                             &current_tm, state.tram14);
        GUI_DisString_EN(20, 140, outp_str, &Font24, LCD_BACKGROUND, BLACK);
    }
    {
        char outp_str[MAX_TRAM_LINE_STRING_LENGTH] = "18: ";
        fill_string_arrivals(outp_str + 4, MAX_TRAM_LINE_STRING_LENGTH - 4,
                             &current_tm, state.tram18);
        GUI_DisString_EN(20, 170, outp_str, &Font24, LCD_BACKGROUND, BLACK);
    }
    {
        char outp_str[MAX_TRAM_LINE_STRING_LENGTH] = "24: ";
        fill_string_arrivals(outp_str + 4, MAX_TRAM_LINE_STRING_LENGTH - 4,
                             &current_tm, state.tram24);
        GUI_DisString_EN(20, 200, outp_str, &Font24, LCD_BACKGROUND, BLACK);
    }
    critical_section_exit(&state.cs);
}
