#pragma once

typedef err_t lwip_err_t;

#define HTTPS_WIFI_TIMEOUT                      20000           // ms

#define HTTPS_HOSTNAME                          "api.open-meteo.com" 
#define HTTPS_QUERY                             "/v1/forecast?latitude=50.07&longitude=14.42&current_weather=true&daily=temperature_2m_max&timezone=auto&forecast_days=1"

#define HTTPS_REQUEST                   \
    "GET " HTTPS_QUERY " HTTP/1.1\r\n"  \
    "Host: " HTTPS_HOSTNAME "\r\n"      \
    "\r\n"

#define HTTPS_RESOLVE_POLL_INTERVAL             100             // ms
#define HTTPS_ALTCP_CONNECT_POLL_INTERVAL       100             // ms
#define HTTPS_ALTCP_IDLE_POLL_INTERVAL          2               // shots
#define HTTPS_HTTP_RESPONSE_POLL_INTERVAL       100             // ms


// Array length
#define LEN(array) (sizeof array)/(sizeof array[0])



/* Data structures ************************************************************/

// TCP connection callback argument
//
//  All callbacks associated with lwIP TCP (+ TLS) connections can be passed a
//  common argument. This is intended to allow application state to be accessed
//  from within the callback context. The argument should be registered with
//  altcp_arg().
//
//  The following structure is used for this argument in order to supply all
//  the relevant application state required by the various callbacks.
//
//  https://www.nongnu.org/lwip/2_1_x/group__altcp.html
//
struct altcp_callback_arg{

    // TCP + TLS connection configurtaion
    //
    //  Memory allocated to the connection configuration structure needs to be
    //  freed (with altcp_tls_free_config) in the connection error callback
    //  (callback_altcp_err).
    //
    //  https://www.nongnu.org/lwip/2_1_x/group__altcp.html
    //  https://www.nongnu.org/lwip/2_1_x/group__altcp__tls.html
    //
    struct altcp_tls_config* config;

    // TCP + TLS connection state
    //
    //  Successful establishment of a connection needs to be signaled to the
    //  application from the connection connect callback
    //  (callback_altcp_connect).
    //
    //  https://www.nongnu.org/lwip/2_1_x/group__altcp.html
    //
    bool connected;

    // Data reception acknowledgement
    //
    //  The amount of data acknowledged as received by the server needs to be
    //  communicated to the application from the connection sent callback
    //  (callback_altcp_sent) for validatation of successful transmission.
    //
    u16_t acknowledged;

};



bool init_stdio(void);
bool init_cyw43(void);
bool connect_to_network(void);
bool resolve_hostname(ip_addr_t* ipaddr);
void altcp_free_pcb(struct altcp_pcb* pcb);
void altcp_free_config(struct altcp_tls_config* config);
void altcp_free_arg(struct altcp_callback_arg* arg);
bool connect_to_host(ip_addr_t* ipaddr, struct altcp_pcb** pcb);
bool send_request(struct altcp_pcb* pcb);
void callback_gethostbyname(
    const char* name,
    const ip_addr_t* resolved,
    void* ipaddr
);
void callback_altcp_err(void* arg, lwip_err_t err);
lwip_err_t callback_altcp_poll(void* arg, struct altcp_pcb* pcb);
lwip_err_t callback_altcp_sent(void* arg, struct altcp_pcb* pcb, u16_t len);
lwip_err_t callback_altcp_recv(
    void* arg,
    struct altcp_pcb* pcb,
    struct pbuf* buf,
    lwip_err_t err
);
lwip_err_t callback_altcp_connect(
    void* arg,
    struct altcp_pcb* pcb,
    lwip_err_t err
);
