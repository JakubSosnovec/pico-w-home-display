#include "pico/sync.h"

#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "LCD_GUI.h"
#include "LCD_Touch.h"

#include "network.h"
#include "tiny-json.h"
#include "weather.h"

#include <string.h>

#define MAX_WEATHER_LINE_STRING_LENGTH 40

struct weather_state {
    critical_section_t cs;
    double current_temp;
    double max_daily_temp;
};
static struct weather_state state;

void init_weather(void) { critical_section_init(&state.cs); }

void update_weather(const char *http_response) {
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

    const json_t *current_weather_field =
        json_getProperty(json, "current_weather");
    assert(json_getType(current_weather_field) == JSON_OBJ);
    const json_t *temperature_field =
        json_getProperty(current_weather_field, "temperature");
    assert(json_getType(temperature_field) == JSON_REAL);
    const json_t *daily_field = json_getProperty(json, "daily");
    assert(json_getType(daily_field) == JSON_OBJ);
    const json_t *temperature_max_arr_field =
        json_getProperty(daily_field, "temperature_2m_max");
    assert(json_getType(temperature_max_arr_field) == JSON_ARRAY);
    const json_t *temperature_max_field =
        json_getChild(temperature_max_arr_field);
    assert(json_getType(temperature_max_field) == JSON_REAL);

    critical_section_enter_blocking(&state.cs);
    state.current_temp = json_getReal(temperature_field);
    state.max_daily_temp = json_getReal(temperature_max_field);
    critical_section_exit(&state.cs);
}

void render_weather(void) {
    critical_section_enter_blocking(&state.cs);
    char temperature_string[MAX_WEATHER_LINE_STRING_LENGTH];
    snprintf(temperature_string, MAX_WEATHER_LINE_STRING_LENGTH,
             "Temp: %.1f C, max: %.1f C", state.current_temp,
             state.max_daily_temp);
    // Include the drawing in the critical section, to avoid interrupts during
    // rendering We first need to reset the LCD in the changed region
    GUI_DrawRectangle(20, 80, 480, 110 + 24, WHITE, DRAW_FULL, DOT_PIXEL_DFT);
    GUI_DisString_EN(20, 80, temperature_string, &Font24, LCD_BACKGROUND, BLUE);
    // GUI_DisString_EN(20, 110, temperature_max_string, &Font24,
    // LCD_BACKGROUND,
    //                 BLUE);
    critical_section_exit(&state.cs);
}
