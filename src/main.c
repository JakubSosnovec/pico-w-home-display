#include "hardware/rtc.h"

#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "LCD_GUI.h"
#include "LCD_Touch.h"

#include "network.h"
#include "rtc.h"
#include "tram.h"
#include "weather.h"

#include "log.h"

#define LEN(array) (sizeof array) / (sizeof array[0])

static void lcd_init(void) {
    log_debug("Initializing LCD");
    DEV_GPIO_Init();
    spi_init(SPI_PORT, 4000000);
    gpio_set_function(LCD_CLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MISO_PIN, GPIO_FUNC_SPI);

    LCD_SCAN_DIR lcd_scan_dir = SCAN_DIR_DFT;
    LCD_Init(lcd_scan_dir, 800);
    TP_Init(lcd_scan_dir);
    GUI_Clear(WHITE);
    GUI_DisString_EN(20, 20, "SOS home assistant", &Font24, LCD_BACKGROUND,
                     RED);
}

static bool timer_callback(repeating_timer_t *rt) {
    void (*f)(void) = rt->user_data;
    f();
    return true; // keep repeating
}

static void render_time_and_tram(void) {
    render_time();
    render_tram();
}

void main(void) {
    stdio_usb_init();
    stdio_set_translate_crlf(&stdio_usb, true);

    init_cyw43();
    lcd_init();
    rtc_init();

    connect_to_wifi(WIFI_SSID, WIFI_PASSWORD);
    set_rtc();

    struct connection_state *weather_connection =
        init_connection(HTTPS_WEATHER_HOSTNAME, WEATHER_TLS_ROOT_CERT,
                        LEN(WEATHER_TLS_ROOT_CERT), HTTPS_WEATHER_REQUEST);
    struct connection_state *tram_connection =
        init_connection(HTTPS_TRAM_HOSTNAME, TRAM_TLS_ROOT_CERT,
                        LEN(TRAM_TLS_ROOT_CERT), HTTPS_TRAM_REQUEST);

    init_tram();
    init_weather();

    // Refresh time and trams every second
    repeating_timer_t timer_time_and_tram;
    if (!add_repeating_timer_ms(1000, timer_callback, render_time_and_tram,
                                &timer_time_and_tram)) {
        log_fatal("Failed to add refresh timer");
        return;
    }

    // Refresh weather every minute
    repeating_timer_t timer_weather;
    if (!add_repeating_timer_ms(60000, timer_callback, render_weather,
                                &timer_weather)) {
        log_fatal("Failed to add refresh timer");
        return;
    }

    // mbedtls_debug_set_threshold(5);

    // We update every 10 seconds
    while (true) {
        if (query_connection(weather_connection)) {
            update_weather(weather_connection->http_response);
        }
        if (query_connection(tram_connection)) {
            update_tram(tram_connection->http_response);
        }

        sleep_ms(10000);
    }

    return;
}
