#include "pico/cyw43_arch.h"

#include "lwip/altcp_tls.h"
#include "lwip/dns.h"
#include "lwip/prot/iana.h" // HTTPS port number

#include "log.h"
#include "main.h"
#include "network.h"

// DNS response callback
static void callback_gethostbyname(const char *name, const ip_addr_t *resolved,
                                   void *ipaddr) {
    if (resolved)
        *(ip_addr_t *)ipaddr = *resolved; // Successful resolution
    else
        ((ip_addr_t *)ipaddr)->addr = IPADDR_NONE; // Failed resolution
}

// TCP + TLS connection error callback
static void callback_altcp_err(void *arg, lwip_err_t err) {
    // Print error code
    log_error("Connection error [lwip_err_t err == %d]", err);
}

// TCP + TLS connection idle callback
static lwip_err_t callback_altcp_poll(void *arg, struct altcp_pcb *pcb) {
    // Callback not currently used
    return ERR_OK;
}

// TCP + TLS data acknowledgement callback
static lwip_err_t callback_altcp_sent(void *arg, struct altcp_pcb *pcb,
                                      u16_t len) {
    struct connection_state *connection = (struct connection_state *)arg;
    connection->send_acknowledged_bytes = len;
    return ERR_OK;
}

// TCP + TLS data reception callback
static lwip_err_t callback_altcp_recv(void *arg, struct altcp_pcb *pcb,
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
static lwip_err_t callback_altcp_connect(void *arg, struct altcp_pcb *pcb,
                                         lwip_err_t err) {
    struct connection_state *connection = (struct connection_state *)arg;
    connection->connected = true;
    return ERR_OK;
}

// Resolve hostname
static void resolve_hostname(ip_addr_t *ipaddr, const char *hostname) {
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
static bool connect_to_host(struct connection_state *connection) {
    log_debug("Connecting to port %d", LWIP_IANA_PORT_HTTPS);
    cyw43_arch_lwip_begin();

    struct altcp_tls_config *config;
    config =
        altcp_tls_create_config_client(connection->cert, connection->cert_len);
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
    altcp_poll(connection->pcb, callback_altcp_poll,
               HTTPS_ALTCP_IDLE_POLL_SHOTS);
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
    lwip_err_t lwip_err =
        altcp_connect(connection->pcb, &connection->ipaddr,
                      LWIP_IANA_PORT_HTTPS, callback_altcp_connect);
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
static bool send_request(struct connection_state *connection) {
    // Write to send buffer
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err = altcp_write(connection->pcb, connection->request,
                                      strlen(connection->request), 0);
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
                connection->send_acknowledged_bytes !=
                    strlen(connection->request))
                lwip_err = -1;
        }
    }

    // Return
    return !((bool)lwip_err);
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

struct connection_state *init_connection(const char *hostname, const char *cert,
                                         size_t cert_len, const char *request) {
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

bool query_connection(struct connection_state *connection) {
    if (!connection->pcb) {
        // Establish new TCP + TLS connection with server
        while (!connect_to_host(connection)) {
            sleep_ms(HTTPS_HTTP_RESPONSE_POLL_INTERVAL_MS);
        }
    }

    connection->received_err = ERR_INPROGRESS;
    connection->http_response_offset = 0;
    memset(connection->http_response, '\0', HTTPS_RESPONSE_MAX_SIZE);
    bool send_success = send_request(connection);
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
        log_warn("Received HTTP response status is not OK. Closing HTTP "
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
