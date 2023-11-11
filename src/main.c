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

static bool timer_callback(repeating_timer_t* rt)
{
    void (*f)(void) = rt->user_data;
    f();
    return true; // keep repeating
}

void main(void) {
    stdio_usb_init();
    stdio_set_translate_crlf(&stdio_usb, true);

    init_cyw43();
    lcd_init();
    rtc_init();

    connect_to_wifi(WIFI_SSID, WIFI_PASSWORD);
    set_rtc();

    // Refresh time every second
    repeating_timer_t timer;
    if (!add_repeating_timer_ms(1000, timer_callback, render_time, &timer)) {
        log_fatal("Failed to add refresh timer");
        return;
    }

    struct connection_state *weather_connection =
        init_connection(HTTPS_WEATHER_HOSTNAME, WEATHER_TLS_ROOT_CERT,
                        LEN(WEATHER_TLS_ROOT_CERT), HTTPS_WEATHER_REQUEST);
    struct connection_state *tram_connection =
        init_connection(HTTPS_TRAM_HOSTNAME, TRAM_TLS_ROOT_CERT,
                        LEN(TRAM_TLS_ROOT_CERT), HTTPS_TRAM_REQUEST);

    // mbedtls_debug_set_threshold(5);

    // We update every 10 seconds
    while (true) {
        if (query_connection(weather_connection)) {
            render_weather(weather_connection->http_response);
        }
        if (query_connection(tram_connection)) {
            render_tram(tram_connection->http_response);
        }

        sleep_ms(10000);
    }

    return;
}
