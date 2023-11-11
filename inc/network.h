#pragma once

#include "pico/cyw43_arch.h"

#include <stdatomic.h>

typedef err_t lwip_err_t;

#define HTTPS_WIFI_TIMEOUT_MS 20000
#define HTTPS_RESPONSE_MAX_SIZE (8 * 1024)
#define HTTPS_RESOLVE_POLL_INTERVAL_MS 100
#define HTTPS_ALTCP_CONNECT_POLL_INTERVAL_MS 100
#define HTTPS_ALTCP_IDLE_POLL_SHOTS 2
#define HTTPS_HTTP_SEND_ACKNOWLEDGE_POLL_INTERVAL_MS 200
#define HTTPS_HTTP_SEND_ACKNOWLEDGE_POLL_SHOTS 20
#define HTTPS_HTTP_RESPONSE_POLL_INTERVAL_MS 100

#define MAX_JSON_FIELDS 200

struct connection_state {
    const char *hostname;
    const char *cert;
    size_t cert_len;
    const char *request;
    struct altcp_tls_config *config;
    struct altcp_pcb *pcb;
    ip_addr_t ipaddr;
    atomic_bool connected;
    atomic_uint send_acknowledged_bytes;
    _Atomic lwip_err_t received_err;
    char http_response[HTTPS_RESPONSE_MAX_SIZE];
    unsigned http_response_offset;
};

void init_cyw43(void);
void connect_to_wifi(const char *ssid, const char *passwd);

struct connection_state *init_connection(const char *hostname, const char *cert,
                                         size_t cert_len, const char *request);
bool query_connection(struct connection_state *connection);
