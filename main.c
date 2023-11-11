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

#include "log.h"
#include "main.h"
#include "time.h"
#include "tiny-json.h"

void main(void) {
    stdio_init();
    init_cyw43();
    lcd_init();
    rtc_init();
    bool rtc_was_set = false; // Was RTC initialized to the current time?

    connect_to_wifi(WIFI_SSID, WIFI_PASSWORD);

    struct connection_state *weather_connection = init_connection(HTTPS_WEATHER_HOSTNAME, WEATHER_TLS_ROOT_CERT, LEN(WEATHER_TLS_ROOT_CERT), HTTPS_WEATHER_REQUEST);
    struct connection_state *tram_connection = init_connection(HTTPS_TRAM_HOSTNAME, TRAM_TLS_ROOT_CERT, LEN(TRAM_TLS_ROOT_CERT), HTTPS_TRAM_REQUEST);

    // mbedtls_debug_set_threshold(5);

    // We update every 10 seconds
    while (true) {
        if(query_connection(weather_connection))
        {
            if (!rtc_was_set) {
                init_rtc(weather_connection->http_response);
                rtc_was_set = true;
            }
            render_weather(weather_connection->http_response);
        }
        if(query_connection(tram_connection))
        {
            render_tram(tram_connection->http_response);
        }

        render_time();
        sleep_ms(10000);
    }

    return;
}

struct connection_state *init_connection(const char* hostname, const char* cert, size_t cert_len, const char* request)
{
    // These are big structs, so rather allocate on heap
    struct connection_state *connection =
        malloc(sizeof(struct connection_state));
    connection->hostname = hostname;
    connection->cert = cert;
    connection->cert_len = cert_len;
    connection->request = request;
    connection->pcb = NULL;

    resolve_hostname(&connection->ipaddr, hostname);
    return connection;
}

bool query_connection(struct connection_state* connection)
{
    if(!connection->pcb)
    {
        // Establish new TCP + TLS connection with server
        while (!connect_to_host(connection)) {
            sleep_ms(HTTPS_HTTP_RESPONSE_POLL_INTERVAL_MS);
        }
    }

    connection->received_err = ERR_INPROGRESS;
    connection->http_response_offset = 0;
    memset(connection->http_response, '\0', HTTPS_RESPONSE_MAX_SIZE);
    bool send_success =
        send_request(connection);
    if (!send_success) {
        log_warn("HTTP request sending failed");
    } else {
        // Await HTTP response
        log_debug("Awaiting HTTP response");
        while (connection->received_err == ERR_INPROGRESS) {
            sleep_ms(HTTPS_HTTP_RESPONSE_POLL_INTERVAL_MS);
        }
        log_info("Got HTTP response");
    }

    if (connection->received_err == ERR_OK) {
        return true;
    } else {
        log_warn(
            "Received HTTP response status is not OK. Closing HTTP "
            "connection");
        // Close connection
        cyw43_arch_lwip_begin();
        altcp_tls_free_config(connection->config);
        altcp_close(connection->pcb);
        cyw43_arch_lwip_end();
        connection->pcb = NULL;
        return false;
    }
}

void init_rtc(const char *http_response) {
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

void render_weather(const char *http_response) {
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

    char temperature_string[30];
    sprintf(temperature_string, "Temperature now: %.1f C",
            json_getReal(temperature_field));
    char temperature_max_string[30];
    sprintf(temperature_max_string, "Temperature max: %.1f C",
            json_getReal(temperature_max_field));

    // We first need to reset the LCD in the changed region
    GUI_DrawRectangle(20, 80, 480, 110 + 24, WHITE, DRAW_FULL, DOT_PIXEL_DFT);
    GUI_DisString_EN(20, 80, temperature_string, &Font24, LCD_BACKGROUND, BLUE);
    GUI_DisString_EN(20, 110, temperature_max_string, &Font24, LCD_BACKGROUND,
                     BLUE);
}

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
        char outp_str[25] = "14| ";
        size_t ind = 4;
        for (size_t i = 0; i < tram14_index; ++i) {
            if (i != 0)
                ind += sprintf(outp_str + ind, ", ");
            if(tram14[i].tm_min != 0)
                ind += sprintf(outp_str + ind, "%dm", tram14[i].tm_min);
            ind += sprintf(outp_str + ind, "%ds", tram14[i].tm_sec);
        }
        GUI_DisString_EN(20, 140, outp_str, &Font24, LCD_BACKGROUND, BLACK);
    }
    {
        char outp_str[25] = "24| ";
        size_t ind = 4;
        for (size_t i = 0; i < tram24_index; ++i) {
            if (i != 0)
                ind += sprintf(outp_str + ind, ", ");
            if(tram24[i].tm_min != 0)
                ind += sprintf(outp_str + ind, "%dm", tram24[i].tm_min);
            ind += sprintf(outp_str + ind, "%ds", tram24[i].tm_sec);
        }
        GUI_DisString_EN(20, 170, outp_str, &Font24, LCD_BACKGROUND, BLACK);
    }
}

void lcd_init(void) {
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

// Initialise standard I/O over USB
void stdio_init(void) {
    stdio_usb_init();
    stdio_set_translate_crlf(&stdio_usb, true);
}

// Initialise Pico W wireless hardware
void init_cyw43(void) {
    log_debug("Initializing CYW43");
    cyw43_arch_init_with_country(CYW43_COUNTRY_CZECH_REPUBLIC);
}

// Connect to wireless network
void connect_to_wifi(const char *ssid, const char *passwd) {
    log_debug("Connecting to wireless network %s with password %s", ssid,
              passwd);
    cyw43_arch_lwip_begin();
    cyw43_arch_enable_sta_mode();
    cyw43_arch_wifi_connect_timeout_ms(ssid, passwd, CYW43_AUTH_WPA2_AES_PSK,
                                       HTTPS_WIFI_TIMEOUT_MS);
    cyw43_arch_lwip_end();
}

// Resolve hostname
void resolve_hostname(ip_addr_t *ipaddr, const char *hostname) {
    log_debug("Resolving %s", hostname);
    ipaddr->addr = IPADDR_ANY;

    // Attempt resolution
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err =
        dns_gethostbyname(hostname, ipaddr, callback_gethostbyname, ipaddr);
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
        else
            log_fatal("Failed to resolve %s", hostname);
    }

    // Convert to ip_addr_t
    cyw43_arch_lwip_begin();
    char *char_ipaddr = ipaddr_ntoa(ipaddr);
    cyw43_arch_lwip_end();
    log_info("Resolved %s (%s)", hostname, char_ipaddr);
}

// Establish TCP + TLS connection with server
bool connect_to_host(struct connection_state *connection)
{
    log_debug("Connecting to port %d", LWIP_IANA_PORT_HTTPS);
    cyw43_arch_lwip_begin();

    struct altcp_tls_config *config;
    config = altcp_tls_create_config_client(connection->cert, connection->cert_len);
    cyw43_arch_lwip_end();
    if (!config) {
        log_fatal("create_config_client failed");
        return false;
    }

    cyw43_arch_lwip_begin();
    connection->pcb = altcp_tls_new(config, IPADDR_TYPE_V4);
    cyw43_arch_lwip_end();
    assert(connection->pcb);

    connection->config = config;
    connection->connected = false;
    cyw43_arch_lwip_begin();
    altcp_arg(connection->pcb, (void *)connection);
    cyw43_arch_lwip_end();

    // Configure connection fatal error callback
    cyw43_arch_lwip_begin();
    altcp_err(connection->pcb, callback_altcp_err);
    cyw43_arch_lwip_end();

    // Configure idle connection callback (and interval)
    cyw43_arch_lwip_begin();
    altcp_poll(connection->pcb, callback_altcp_poll, HTTPS_ALTCP_IDLE_POLL_SHOTS);
    cyw43_arch_lwip_end();

    // Configure data acknowledge callback
    cyw43_arch_lwip_begin();
    altcp_sent(connection->pcb, callback_altcp_sent);
    cyw43_arch_lwip_end();

    // Configure data reception callback
    cyw43_arch_lwip_begin();
    altcp_recv(connection->pcb, callback_altcp_recv);
    cyw43_arch_lwip_end();

    // Send connection request (SYN)
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err = altcp_connect(connection->pcb, &connection->ipaddr, LWIP_IANA_PORT_HTTPS,
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
        while (!(connection->connected))
            sleep_ms(HTTPS_ALTCP_CONNECT_POLL_INTERVAL_MS);
        log_info("HTTP SYN-ACK packet received successfully");
    } else {
        log_warn("HTTP SYN packet sending failed");
    }

    // Return
    return !((bool)lwip_err);
}

// Send HTTP request
bool send_request(struct connection_state *connection)
{
    // Write to send buffer
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err = altcp_write(connection->pcb, connection->request, strlen(connection->request), 0);
    cyw43_arch_lwip_end();

    // Written to send buffer
    if (lwip_err == ERR_OK) {

        // Output send buffer
        connection->send_acknowledged_bytes = 0;
        cyw43_arch_lwip_begin();
        lwip_err = altcp_output(connection->pcb);
        cyw43_arch_lwip_end();

        // Send buffer output
        if (lwip_err == ERR_OK) {
            // Await acknowledgement
            unsigned shots = 0;
            for (; connection->send_acknowledged_bytes == 0 &&
                   shots < HTTPS_HTTP_SEND_ACKNOWLEDGE_POLL_SHOTS;
                 ++shots)
                sleep_ms(HTTPS_HTTP_SEND_ACKNOWLEDGE_POLL_INTERVAL_MS);
            if (shots == HTTPS_HTTP_SEND_ACKNOWLEDGE_POLL_SHOTS ||
                connection->send_acknowledged_bytes != strlen(connection->request))
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
    log_error("Connection error [lwip_err_t err == %d]", err);
}

// TCP + TLS connection idle callback
lwip_err_t callback_altcp_poll(void *arg, struct altcp_pcb *pcb) {
    // Callback not currently used
    return ERR_OK;
}

// TCP + TLS data acknowledgement callback
lwip_err_t callback_altcp_sent(void *arg, struct altcp_pcb *pcb, u16_t len) {
    struct connection_state *connection = (struct connection_state *)arg;
    connection->send_acknowledged_bytes = len;
    return ERR_OK;
}

// TCP + TLS data reception callback
lwip_err_t callback_altcp_recv(void *arg, struct altcp_pcb *pcb,
                               struct pbuf *buf, lwip_err_t err) {
    struct connection_state *connection = (struct connection_state *)arg;
    struct pbuf *head = buf;

    log_trace("Received HTTP response:");
    if (err == ERR_OK) {
        if (head == NULL || head->tot_len == 0)
            return ERR_OK;

        assert(connection->http_response_offset + head->tot_len <
               HTTPS_RESPONSE_MAX_SIZE);
        while (buf) {
            for (size_t i = 0; i < buf->len; ++i) {
                putchar(((char *)buf->payload)[i]);
                connection
                    ->http_response[connection->http_response_offset + i] =
                    ((char *)buf->payload)[i];
            }
            connection->http_response_offset += buf->len;
            buf = buf->next;
        }

        // Advertise data reception
        altcp_recved(pcb, head->tot_len);

        // Free buf
        pbuf_free(head); // Free entire pbuf chain

        // If the number of { and } are the same in the response, this is
        // the last chunk
        size_t left = 0, right = 0;
        for (size_t i = 0; i < connection->http_response_offset; ++i) {
            if (connection->http_response[i] == '{')
                ++left;
            if (connection->http_response[i] == '}')
                ++right;
        }
        if (left == right)
            connection->received_err = err;
    } else {
        connection->received_err = err;
    }
    return ERR_OK;
}

// TCP + TLS connection establishment callback
lwip_err_t callback_altcp_connect(void *arg, struct altcp_pcb *pcb,
                                  lwip_err_t err) {
    struct connection_state *connection = (struct connection_state *)arg;
    connection->connected = true;
    return ERR_OK;
}
