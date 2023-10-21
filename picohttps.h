/* Pico HTTPS request example *************************************************
 *                                                                            *
 *  An HTTPS client example for the Raspberry Pi Pico W                       *
 *                                                                            *
 *  A simple yet complete example C application which sends a single request  *
 *  to a web server over HTTPS and reads the resulting response.              *
 *                                                                            *
 ******************************************************************************/

#ifndef PICOHTTPS_H
#define PICOHTTPS_H



/* Options ********************************************************************/

// Wireless region
//
//  `country` argument to cyw43_arch_init_with_country().
//
//  For best performance, set to local region.
//
//  https://www.raspberrypi.com/documentation/pico-sdk/networking.html#CYW43_COUNTRY_
//
#define PICOHTTPS_INIT_CYW43_COUNTRY                CYW43_COUNTRY_CZECH_REPUBLIC

// Wireless network SSID
#define PICOHTTPS_WIFI_SSID                         WIFI_SSID

// Wireless network connection timeout
//
//  `timeout` argument to cyw43_arch_wifi_connect_timeout_ms().
//
//  https://www.raspberrypi.com/documentation/pico-sdk/networking.html
//
#define PICOHTTPS_WIFI_TIMEOUT                      20000           // ms

// Wireless network password
//
//  N.b. _Strongly_ recommend setting this from the environment rather than
//  here. Environment values will have greater precedence. See CMakeLists.txt.
//
#ifndef PICOHTTPS_WIFI_PASSWORD
#define PICOHTTPS_WIFI_PASSWORD                     WIFI_PASSWORD
#endif // PICOHTTPS_WIFI_PASSWORD

// HTTP server hostname
// Home coordinates
#define PICOHTTPS_HOSTNAME                          "api.open-meteo.com" 

#define PICOHTTPS_QUERY                             "/v1/forecast?latitude=50.07&longitude=14.42&current_weather=true&hourly=temperature_2m"

// DNS response polling interval
//
//  Interval with which to poll for responses to DNS queries.
//
#define PICOHTTPS_RESOLVE_POLL_INTERVAL             100             // ms

// TCP + TLS connection establishment polling interval
//
//  Interval with which to poll for establishment of TCP + TLS connection
//
#define PICOHTTPS_ALTCP_CONNECT_POLL_INTERVAL       100             // ms

// TCP + TLS idle connection polling interval
//
//  Interval with which to poll application (i.e. call registered polling
//  callback function) when TCP + TLS connection is idle.
//
//  The callback function should be registered with altcp_poll(). The polling
//  interval is given in units of 'coarse grain timer shots'; one shot
//  corresponds to approximately 500 ms.
//
//  https://www.nongnu.org/lwip/2_1_x/group__altcp.html
//
#define PICOHTTPS_ALTCP_IDLE_POLL_INTERVAL          2               // shots

// HTTP request
//
//  Plain-text HTTP request to send to server
//
#define PICOHTTPS_REQUEST                   \
    "GET " PICOHTTPS_QUERY " HTTP/1.1\r\n"  \
    "Host: " PICOHTTPS_HOSTNAME "\r\n"      \
    "\r\n"


// HTTP response polling interval
//
//  Interval with which to poll for HTTP response from server.
//
#define PICOHTTPS_HTTP_RESPONSE_POLL_INTERVAL       100             // ms



/* Macros *********************************************************************/

// Array length
#define LEN(array) (sizeof array)/(sizeof array[0])



/* Data structures ************************************************************/

// lwIP errors
//
//  typedef here to make source of error code more explicit
//
typedef err_t lwip_err_t;

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



/* Functions ******************************************************************/

// Initialise standard I/O over USB
//
//  @return         `true` on success
//
bool init_stdio(void);

// Initialise Pico W wireless hardware
//
//  @return         `true` on success
//
bool init_cyw43(void);

// Connect to wireless network
//
//  @return         `true` on success
//
bool connect_to_network(void);

// Resolve hostname
//
//  @param ipaddr   Pointer to an `ip_addr_t` where the resolved IP address
//                  should be stored.
//
//  @return         `true` on success
//
bool resolve_hostname(ip_addr_t* ipaddr);

// Free TCP + TLS protocol control block
//
//  Memory allocated for a protocol control block (with altcp_tls_new) needs to
//  be freed (with altcp_close).
//
//  @param pcb      Pointer to a `altcp_pcb` structure to be freed
//
void altcp_free_pcb(struct altcp_pcb* pcb);

// Free TCP + TLS connection configuration
//
//  Memory allocated for TCP + TLS connection configuration (with
//  altcp_tls_create_config_client) needs to be freed (with
//  altcp_tls_free_config).
//
//  @param config   Pointer to a `altcp_tls_config` structure to be freed
//
void altcp_free_config(struct altcp_tls_config* config);

// Free TCP + TLS connection callback argument
//
//  The common argument passed to lwIP connection callbacks must remain in
//  scope for the duration of all callback contexts (i.e. connection lifetime).
//  As such, it cannot be declared with function scope when registering the
//  callback, but rather should be allocated on the heap. This implies the
//  allocated memory must be freed on connection close.
//
//  @param arg      Pointer to a `altcp_callback_arg` structure to be freed
//
void altcp_free_arg(struct altcp_callback_arg* arg);

// Establish TCP + TLS connection with server
//
//  @param ipaddr   Pointer to an `ip_addr_t` containing the server's IP
//                  address
//  @param pcb      Double pointer to a `altcp_pcb` structure where the
//                  protocol control block for the established connection
//                  should be stored.
//
//  @return         `true` on success
//
bool connect_to_host(ip_addr_t* ipaddr, struct altcp_pcb** pcb);

// Send HTTP request
//
//  @param pcb      Pointer to a `altcp_pcb` structure containing the TCP + TLS
//                  connection PCB to the server.
//
//  @return         `true` on success
//
bool send_request(struct altcp_pcb* pcb);

// DNS response callback
//
//  Callback function fired on DNS query response.
//
//  Registered with dns_gethostbyname().
//
//  https://www.nongnu.org/lwip/2_1_x/group__dns.html
//
void callback_gethostbyname(
    const char* name,
    const ip_addr_t* resolved,
    void* ipaddr
);

// TCP + TLS connection error callback
//
//  Callback function fired on TCP + TLS connection fatal error.
//
//  Registered with altcp_err().
//
//  https://www.nongnu.org/lwip/2_1_x/group__altcp.html
//
void callback_altcp_err(void* arg, lwip_err_t err);

// TCP + TLS connection idle callback
//
//  Callback function fired on idle TCP + TLS connection.
//
//  Registered with altcp_err().
//
//  https://www.nongnu.org/lwip/2_1_x/group__altcp.html
//
lwip_err_t callback_altcp_poll(void* arg, struct altcp_pcb* pcb);

// TCP + TLS data acknowledgement callback
//
//  Callback function fired on acknowledgement of data reception by server over
//  a TCP + TLS connection.
//
//  Registered with altcp_sent().
//
//  https://www.nongnu.org/lwip/2_1_x/group__altcp.html
//
lwip_err_t callback_altcp_sent(void* arg, struct altcp_pcb* pcb, u16_t len);

// TCP + TLS data reception callback
//
//  Callback function fired on reception of data from server over a TCP +
//  TLS connection.
//
//  Registered with altcp_recv().
//
//  https://www.nongnu.org/lwip/2_1_x/group__altcp.html
//
lwip_err_t callback_altcp_recv(
    void* arg,
    struct altcp_pcb* pcb,
    struct pbuf* buf,
    lwip_err_t err
);

// TCP + TLS connection establishment callback
//
//  Callback function fired on successful establishment of TCP + TLS connection.
//
//  Registered with altcp_connect().
//
//  https://www.nongnu.org/lwip/2_1_x/group__altcp.html
//
lwip_err_t callback_altcp_connect(
    void* arg,
    struct altcp_pcb* pcb,
    lwip_err_t err
);



#endif //PICOHTTPS_H
