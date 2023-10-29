#include "hardware/rtc.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"

#include "lwip/altcp_tls.h"
#include "lwip/dns.h"
#include "lwip/prot/iana.h" // HTTPS port number

#include "DEV_Config.h"
#include "LCD_Bmp.h"
#include "LCD_Driver.h"
#include "LCD_GUI.h"
#include "LCD_Touch.h"

#include "time.h"

#include "log.h"
#include "main.h"

void main(void) {
    rtc_init();
    init_stdio();

    log_debug("Initializing CYW43");
    init_cyw43();

    log_debug("Initializing LCD");
    init_lcd();

    log_debug("Connecting to wireless network %s", WIFI_SSID);
    connect_to_network();

    // Resolve server hostname
    ip_addr_t ipaddr;
    char *char_ipaddr;
    log_debug("Resolving %s", HTTPS_WEATHER_HOSTNAME);
    if (!resolve_hostname(&ipaddr)) {
        log_fatal("Failed to resolve %s", HTTPS_WEATHER_HOSTNAME);
        return;
    }

    // Convert to ip_addr_t
    cyw43_arch_lwip_begin();
    char_ipaddr = ipaddr_ntoa(&ipaddr);
    cyw43_arch_lwip_end();
    log_info("Resolved %s (%s)", HTTPS_WEATHER_HOSTNAME, char_ipaddr);

    bool rtc_initialized = false;
    while (true) {
        // Establish TCP + TLS connection with server
        struct altcp_callback_arg callback_arg;
        struct altcp_pcb *pcb = NULL;
        log_debug("Connecting to https://%s:%d", char_ipaddr,
                  LWIP_IANA_PORT_HTTPS);
        if (!connect_to_host(&ipaddr, &pcb, &callback_arg)) {
            log_fatal("Failed to connect to https://%s:%d", char_ipaddr,
                      LWIP_IANA_PORT_HTTPS);
            return;
        }
        log_info("Connected to https://%s:%d", char_ipaddr,
                 LWIP_IANA_PORT_HTTPS);

        // We update every 10 seconds
        while (true) {
            callback_arg.received_err = ERR_INPROGRESS;
            const bool send_success = send_request(pcb, &callback_arg);
            if (!send_success) {
                log_warn("HTTPS request sending failed");
            } else {
                // Await HTTP response
                log_debug("Awaiting HTTPSresponse");
                while (callback_arg.received_err == ERR_INPROGRESS) {
                    sleep_ms(HTTPS_HTTP_RESPONSE_POLL_INTERVAL_MS);
                }
                log_info("Got HTTPS response");
            }

            if (callback_arg.received_err == ERR_OK) {
                if (!rtc_initialized) {
                    init_rtc(callback_arg.http_response);
                    rtc_initialized = true;
                }
                render_time(callback_arg.http_response);
                render_temperature(callback_arg.http_response);
            } else {
                log_warn("Received HTTPS response status is not OK");
                break;
            }

            sleep_ms(10000);
        }
        log_info("Closing HTTPS connection");
        cyw43_arch_lwip_begin();
        altcp_tls_free_config(callback_arg.config);
        altcp_close(pcb);
        cyw43_arch_lwip_end();
    }

    return;
}

void init_rtc(const char *http_response) {
    const char *datetime_tag = "Date: ";
    const char *end_tag = " GMT";
    const char *datetime_ptr = strstr(http_response, datetime_tag);
    assert(datetime_ptr);
    datetime_ptr += strlen(datetime_tag);
    const char *end_ptr = strstr(http_response, end_tag);
    assert(end_ptr);
    unsigned n = end_ptr - datetime_ptr;

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

void render_time() {
    datetime_t t;
    rtc_get_datetime(&t);
    char datetime_str[40];
    sprintf(datetime_str, "%d:%02d:%02d %d/%d %d", t.hour, t.min, t.sec, t.day,
            t.month, t.year + 1900);

    // We first need to reset the LCD in the changed region
    GUI_DrawRectangle(20, 110, 480, 140 + 24, WHITE, DRAW_FULL, DOT_PIXEL_DFT);
    GUI_DisString_EN(20, 110, datetime_str, &Font24, LCD_BACKGROUND, BLACK);
}

void render_temperature(const char *http_response) {
    const char *current_json_value;
    const char *max_json_value;
    const char *current_json_tag = "\"temperature\":";
    const char *max_json_tag = "\"temperature_2m_max\":";
    unsigned i;

    current_json_value =
        strstr(http_response,
               current_json_tag); // First occurence is the units
    assert(current_json_value);
    current_json_value =
        strstr(current_json_value + 1,
               current_json_tag); // Second occurence is the actual value
    current_json_value += strlen(current_json_tag);

    max_json_value = strstr(http_response,
                            max_json_tag); // First occurence is the units
    assert(max_json_value);
    max_json_value =
        strstr(max_json_value + 1,
               max_json_tag); // Second occurence is the actual value
    max_json_value += strlen(max_json_tag) + 1; // +1 for the [

    char current_value[strlen("10.0") + 1];
    for (i = 0; current_json_value[i] != ','; ++i) {
        current_value[i] = current_json_value[i];
    }
    current_value[i] = '\0';

    char max_value[strlen("10.0") + 1];
    for (i = 0; max_json_value[i] != ']'; ++i) {
        max_value[i] = max_json_value[i];
    }
    max_value[i] = '\0';

    char current_string[] = "Temperature now: XXXX  ";
    sprintf(current_string, "Temperature now: %s C", current_value);

    char max_string[] = "Temperature max: XXXX  ";
    sprintf(max_string, "Temperature max: %s C", max_value);

    // We first need to reset the LCD in the changed region
    GUI_DrawRectangle(20, 50, 480, 80 + 24, WHITE, DRAW_FULL, DOT_PIXEL_DFT);
    GUI_DisString_EN(20, 50, current_string, &Font24, LCD_BACKGROUND, BLUE);
    GUI_DisString_EN(20, 80, max_string, &Font24, LCD_BACKGROUND, BLUE);
}

void init_lcd(void) {
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

// Initialise standard I/O over USB
void init_stdio(void) {
    stdio_usb_init();
    stdio_set_translate_crlf(&stdio_usb, true);
}

// Initialise Pico W wireless hardware
void init_cyw43(void) {
    cyw43_arch_init_with_country(CYW43_COUNTRY_CZECH_REPUBLIC);
}

// Connect to wireless network
void connect_to_network(void) {
    cyw43_arch_lwip_begin();
    cyw43_arch_enable_sta_mode();
    cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                       CYW43_AUTH_WPA2_AES_PSK,
                                       HTTPS_WIFI_TIMEOUT_MS);
    cyw43_arch_lwip_end();
}

// Resolve hostname
bool resolve_hostname(ip_addr_t *ipaddr) {

    // Zero address
    ipaddr->addr = IPADDR_ANY;

    // Attempt resolution
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err = dns_gethostbyname(HTTPS_WEATHER_HOSTNAME, ipaddr,
                                            callback_gethostbyname, ipaddr);
    cyw43_arch_lwip_end();
    if (lwip_err == ERR_INPROGRESS) {

        // Await resolution
        //
        //  IP address will be made available shortly (by callback) upon
        //  DNS query response.
        //
        while (ipaddr->addr == IPADDR_ANY)
            sleep_ms(HTTPS_RESOLVE_POLL_INTERVAL_MS);
        if (ipaddr->addr != IPADDR_NONE)
            lwip_err = ERR_OK;
    }

    // Return
    return !((bool)lwip_err);
}

// Establish TCP + TLS connection with server
bool connect_to_host(ip_addr_t *ipaddr, struct altcp_pcb **pcb,
                     struct altcp_callback_arg *arg) {

    const u8_t cert[] = TLS_ROOT_CERT;
    cyw43_arch_lwip_begin();
    struct altcp_tls_config *config =
        altcp_tls_create_config_client(cert, LEN(cert));
    cyw43_arch_lwip_end();
    if (!config) {
        log_fatal("create_config_client failed");
        return false;
    }

    // Instantiate connection PCB
    //
    //  Can also do this more generically using;
    //
    //    altcp_allocator_t allocator = {
    //      altcp_tls_alloc,       // Allocator function
    //      config                 // Allocator function argument (state)
    //    };
    //    altcp_new(&allocator);
    //
    //  No benefit in doing this though; altcp_tls_alloc calls altcp_tls_new
    //  under the hood anyway.
    //
    cyw43_arch_lwip_begin();
    *pcb = altcp_tls_new(config, IPADDR_TYPE_V4);
    cyw43_arch_lwip_end();
    assert(*pcb);

    arg->config = config;
    arg->connected = false;
    cyw43_arch_lwip_begin();
    altcp_arg(*pcb, (void *)arg);
    cyw43_arch_lwip_end();

    // Configure connection fatal error callback
    cyw43_arch_lwip_begin();
    altcp_err(*pcb, callback_altcp_err);
    cyw43_arch_lwip_end();

    // Configure idle connection callback (and interval)
    cyw43_arch_lwip_begin();
    altcp_poll(*pcb, callback_altcp_poll, HTTPS_ALTCP_IDLE_POLL_SHOTS);
    cyw43_arch_lwip_end();

    // Configure data acknowledge callback
    cyw43_arch_lwip_begin();
    altcp_sent(*pcb, callback_altcp_sent);
    cyw43_arch_lwip_end();

    // Configure data reception callback
    cyw43_arch_lwip_begin();
    altcp_recv(*pcb, callback_altcp_recv);
    cyw43_arch_lwip_end();

    // Send connection request (SYN)
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err = altcp_connect(*pcb, ipaddr, LWIP_IANA_PORT_HTTPS,
                                        callback_altcp_connect);
    cyw43_arch_lwip_end();

    // Connection request sent
    if (lwip_err == ERR_OK) {
        log_info("HTTP SYN packet sent successfully, awaiting response");

        // Await connection
        //
        //  Sucessful connection will be confirmed shortly in
        //  callback_altcp_connect.
        //
        while (!(arg->connected))
            sleep_ms(HTTPS_ALTCP_CONNECT_POLL_INTERVAL_MS);
    } else {
        log_warn("HTTP SYN packet sending failed");
    }

    // Return
    return !((bool)lwip_err);
}

// Send HTTP request
bool send_request(struct altcp_pcb *pcb,
                  struct altcp_callback_arg *callback_arg) {
    const char request[] = HTTPS_WEATHER_REQUEST;

    // Check send buffer and queue length
    //
    //  Docs state that altcp_write() returns ERR_MEM on send buffer too small
    //  _or_ send queue too long. Could either check both before calling
    //  altcp_write, or just handle returned ERR_MEM — which is preferable?
    //
    // if(
    //  altcp_sndbuf(pcb) < (LEN(request) - 1)
    //  || altcp_sndqueuelen(pcb) > TCP_SND_QUEUELEN
    //) return -1;

    // Write to send buffer
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err = altcp_write(pcb, request, LEN(request) - 1, 0);
    cyw43_arch_lwip_end();

    // Written to send buffer
    if (lwip_err == ERR_OK) {

        // Output send buffer
        callback_arg->send_acknowledged_bytes = 0;
        cyw43_arch_lwip_begin();
        lwip_err = altcp_output(pcb);
        cyw43_arch_lwip_end();

        // Send buffer output
        if (lwip_err == ERR_OK) {
            // Await acknowledgement
            unsigned shots = 0;
            for (; callback_arg->send_acknowledged_bytes == 0 &&
                   shots < HTTPS_HTTP_RESPONSE_POLL_SHOTS;
                 ++shots)
                sleep_ms(HTTPS_HTTP_RESPONSE_POLL_INTERVAL_MS);
            if (shots == HTTPS_HTTP_RESPONSE_POLL_SHOTS ||
                callback_arg->send_acknowledged_bytes != (LEN(request) - 1))
                lwip_err = -1;
        }
    }

    // Return
    return !((bool)lwip_err);
}

// DNS response callback
void callback_gethostbyname(const char *name, const ip_addr_t *resolved,
                            void *ipaddr) {
    if (resolved)
        *((ip_addr_t *)ipaddr) = *resolved; // Successful resolution
    else
        ((ip_addr_t *)ipaddr)->addr = IPADDR_NONE; // Failed resolution
}

// TCP + TLS connection error callback
void callback_altcp_err(void *arg, lwip_err_t err) {
    // Print error code
    log_fatal("Connection error [lwip_err_t err == %d]", err);
}

// TCP + TLS connection idle callback
lwip_err_t callback_altcp_poll(void *arg, struct altcp_pcb *pcb) {
    // Callback not currently used
    return ERR_OK;
}

// TCP + TLS data acknowledgement callback
lwip_err_t callback_altcp_sent(void *arg, struct altcp_pcb *pcb, u16_t len) {
    struct altcp_callback_arg *callback_arg = (struct altcp_callback_arg *)arg;
    callback_arg->send_acknowledged_bytes = len;
    return ERR_OK;
}

// TCP + TLS data reception callback
lwip_err_t callback_altcp_recv(void *arg, struct altcp_pcb *pcb,
                               struct pbuf *buf, lwip_err_t err) {
    struct altcp_callback_arg *callback_arg = (struct altcp_callback_arg *)arg;
    struct pbuf *head = buf;

    log_trace("Received HTTP response:");
    if (err == ERR_OK && head) {
        assert(head->tot_len < HTTPS_RESPONSE_MAX_SIZE);
        memset(callback_arg->http_response, '\0', HTTPS_RESPONSE_MAX_SIZE);
        unsigned j = 0;
        while (buf) {
            for (unsigned i = 0; i < buf->len; ++i, ++j) {
                putchar(((char *)buf->payload)[i]);
                callback_arg->http_response[j] = ((char *)buf->payload)[i];
            }
            buf = buf->next;
        }

        // Advertise data reception
        altcp_recved(pcb, head->tot_len);

        // Free buf
        pbuf_free(head); // Free entire pbuf chain
    }

    callback_arg->received_err = err;
    return ERR_OK;
}

// TCP + TLS connection establishment callback
lwip_err_t callback_altcp_connect(void *arg, struct altcp_pcb *pcb,
                                  lwip_err_t err) {
    struct altcp_callback_arg *callback_arg = (struct altcp_callback_arg *)arg;
    callback_arg->connected = true;
    return ERR_OK;
}
