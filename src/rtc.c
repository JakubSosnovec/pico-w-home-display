/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
// Modifications copyright 2023 Jakub Sosnovec

#include "hardware/rtc.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"

#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "LCD_GUI.h"
#include "LCD_Touch.h"

#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "rtc.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>

// This file is taken almost completely from
// https://github.com/raspberrypi/pico-examples/blob/master/pico_w/wifi/ntp_client/picow_ntp_client.c

typedef struct NTP_T_ {
    ip_addr_t ntp_server_address;
    bool dns_request_sent;
    struct udp_pcb *ntp_pcb;
    alarm_id_t ntp_resend_alarm;
    atomic_bool done;
} NTP_T;

#define NTP_SERVER "pool.ntp.org"
#define NTP_MSG_LEN 48
#define NTP_PORT 123
#define NTP_DELTA 2208988800 // seconds between 1 Jan 1900 and 1 Jan 1970
#define NTP_RESEND_TIME (10 * 1000)

// Called with results of operation
static void ntp_result(NTP_T *state, int status, time_t *result) {
    if (status == 0 && result) {
        struct tm tm;
        gmtime_r(result, &tm);
        log_info("Got NTP response: %02d/%02d/%04d %02d:%02d:%02d", tm.tm_mday,
               tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min,
               tm.tm_sec);

        // Manual timezone fix, since doing this with plain libc is bloody
        // frustrating
        tm.tm_hour++;
        mktime(&tm); // Should fix up the time into valid values

        datetime_t t = {.year = tm.tm_year + 1900,
                        .month = tm.tm_mon + 1,
                        .day = tm.tm_mday,
                        .dotw = tm.tm_wday,
                        .hour = tm.tm_hour,
                        .min = tm.tm_min,
                        .sec = tm.tm_sec};
        rtc_set_datetime(&t);
        state->done = true;
    }

    if (state->ntp_resend_alarm > 0) {
        cancel_alarm(state->ntp_resend_alarm);
        state->ntp_resend_alarm = 0;
    }
    state->dns_request_sent = false;
}

// Make an NTP request
static void ntp_request(NTP_T *state) {
    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure
    // correct locking. You can omit them if you are in a callback from lwIP.
    // Note that when using pico_cyw_arch_poll these calls are a no-op and can
    // be omitted, but it is a good practice to use them in case you switch the
    // cyw43_arch type later.
    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    uint8_t *req = (uint8_t *)p->payload;
    memset(req, 0, NTP_MSG_LEN);
    req[0] = 0x1b;
    udp_sendto(state->ntp_pcb, p, &state->ntp_server_address, NTP_PORT);
    pbuf_free(p);
    cyw43_arch_lwip_end();
}

static int64_t ntp_failed_handler(alarm_id_t id, void *user_data) {
    NTP_T *state = (NTP_T *)user_data;
    log_error("ntp request failed");
    ntp_result(state, -1, NULL);
    return 0;
}

// Call back with a DNS result
static void ntp_dns_found(const char *hostname, const ip_addr_t *ipaddr,
                          void *arg) {
    NTP_T *state = (NTP_T *)arg;
    if (ipaddr) {
        state->ntp_server_address = *ipaddr;
        log_info("Resolved NTP address: %s", ipaddr_ntoa(ipaddr));
        ntp_request(state);
    } else {
        log_error("NTP DNS request failed");
        ntp_result(state, -1, NULL);
    }
}

// NTP data received
static void ntp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                     const ip_addr_t *addr, u16_t port) {
    NTP_T *state = (NTP_T *)arg;
    uint8_t mode = pbuf_get_at(p, 0) & 0x7;
    uint8_t stratum = pbuf_get_at(p, 1);

    // Check the result
    if (ip_addr_cmp(addr, &state->ntp_server_address) && port == NTP_PORT &&
        p->tot_len == NTP_MSG_LEN && mode == 0x4 && stratum != 0) {
        uint8_t seconds_buf[4] = {0};
        pbuf_copy_partial(p, seconds_buf, sizeof(seconds_buf), 40);
        uint32_t seconds_since_1900 = seconds_buf[0] << 24 |
                                      seconds_buf[1] << 16 |
                                      seconds_buf[2] << 8 | seconds_buf[3];
        uint32_t seconds_since_1970 = seconds_since_1900 - NTP_DELTA;
        time_t epoch = seconds_since_1970;
        ntp_result(state, 0, &epoch);
    } else {
        log_error("Invalid NTP response");
        ntp_result(state, -1, NULL);
    }
    pbuf_free(p);
}

// Perform initialisation
static NTP_T *ntp_init(void) {
    NTP_T *state = (NTP_T *)calloc(1, sizeof(NTP_T));
    if (!state) {
        log_error("Failed to allocate state");
        return NULL;
    }
    state->ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!state->ntp_pcb) {
        log_error("Failed to create pcb");
        free(state);
        return NULL;
    }
    udp_recv(state->ntp_pcb, ntp_recv, state);
    return state;
}

void set_rtc(void) {
    NTP_T *state = ntp_init();
    assert(state);

    while (!state->done) {
        if (!state->dns_request_sent) {
            // Set alarm in case udp requests are lost
            state->ntp_resend_alarm = add_alarm_in_ms(
                NTP_RESEND_TIME, ntp_failed_handler, state, true);

            // cyw43_arch_lwip_begin/end should be used around calls into lwIP
            // to ensure correct locking. You can omit them if you are in a
            // callback from lwIP. Note that when using pico_cyw_arch_poll these
            // calls are a no-op and can be omitted, but it is a good practice
            // to use them in case you switch the cyw43_arch type later.
            cyw43_arch_lwip_begin();
            int err = dns_gethostbyname(NTP_SERVER, &state->ntp_server_address,
                                        ntp_dns_found, state);
            cyw43_arch_lwip_end();

            state->dns_request_sent = true;
            if (err == ERR_OK) {
                ntp_request(state);             // Cached result
            } else if (err != ERR_INPROGRESS) { // ERR_INPROGRESS means expect a
                                                // callback
                log_error("DNS request failed");
                ntp_result(state, -1, NULL);
            }
        }
        sleep_ms(100);
    }
    free(state);
}

void render_time(void) {
    datetime_t t;
    rtc_get_datetime(&t);
    char datetime_str[40];
    sprintf(datetime_str, "%d:%02d:%02d, %d/%d, %d", t.hour, t.min, t.sec,
            t.day, t.month, t.year);

    // We first need to reset the LCD in the changed region
    GUI_DrawRectangle(20, 50, 480, 50 + 24, WHITE, DRAW_FULL, DOT_PIXEL_DFT);
    GUI_DisString_EN(20, 50, datetime_str, &Font24, LCD_BACKGROUND, BLACK);
}
