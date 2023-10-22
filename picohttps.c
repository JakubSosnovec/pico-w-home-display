/* Pico HTTPS request example *************************************************
 *                                                                            *
 *  An HTTPS client example for the Raspberry Pi Pico W                       *
 *                                                                            *
 *  A simple yet complete example C application which sends a single request  *
 *  to a web server over HTTPS and reads the resulting response.              *
 *                                                                            *
 ******************************************************************************/


/* Includes *******************************************************************/

// Pico SDK
#include "pico/stdlib.h"            // Standard library
#include "pico/cyw43_arch.h"        // Pico W wireless

// lwIP
#include "lwip/dns.h"               // Hostname resolution
#include "lwip/altcp_tls.h"         // TCP + TLS (+ HTTP == HTTPS)
#include "lwip/prot/iana.h"         // HTTPS port number

// Pico HTTPS request example
#include "picohttps.h"              // Options, macros, forward declarations

// LCD SDK
#include "LCD_Driver.h"
#include "LCD_Touch.h"
#include "LCD_GUI.h"
#include "LCD_Bmp.h"
#include "DEV_Config.h"

#define TLS_ROOT_CERT "-----BEGIN CERTIFICATE-----\n\
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n\
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n\
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n\
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n\
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n\
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n\
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n\
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n\
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n\
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n\
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n\
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n\
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n\
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n\
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n\
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n\
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n\
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n\
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n\
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n\
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n\
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n\
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n\
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n\
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n\
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n\
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n\
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n\
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n\
-----END CERTIFICATE-----\n"

void send(ip_addr_t ipaddr, char* char_ipaddr, struct altcp_pcb* pcb);
void init_lcd(void);
void render_temperature(void);
void render_time(void);

/* Main ***********************************************************************/

#define RESPONSE_SIZE 4096
char* weather_info;
int weather_info_size;

void main(void){
    // Initialise standard I/O over USB
    if(!init_stdio()) return;

    // Initialise Pico W wireless hardware
    printf("Initializing CYW43\n");
    if(!init_cyw43()){
        printf("Failed to initialize CYW43\n");
        return;
    }
    printf("Initialized CYW43\n");

    // Connect to wireless network
    printf("Connecting to %s\n", WIFI_SSID);
    if(!connect_to_network()){
        printf("Failed to connect to %s\n", WIFI_SSID);
        cyw43_arch_deinit();            // Deinit Pico W wireless hardware
        return;
    }
    printf("Connected to %s\n", WIFI_SSID);

    // Resolve server hostname
    ip_addr_t ipaddr;
    char* char_ipaddr;
    printf("Resolving %s\n", PICOHTTPS_HOSTNAME);
    if(!resolve_hostname(&ipaddr)){
        printf("Failed to resolve %s\n", PICOHTTPS_HOSTNAME);
                                        // TODO: Disconnect from network
        cyw43_arch_deinit();            // Deinit Pico W wireless hardware
        return;
    }
    cyw43_arch_lwip_begin();
    char_ipaddr = ipaddr_ntoa(&ipaddr);
    cyw43_arch_lwip_end();
    printf("Resolved %s (%s)\n", PICOHTTPS_HOSTNAME, char_ipaddr);

    // Establish TCP + TLS connection with server
    struct altcp_pcb* pcb = NULL;
    printf("Connecting to https://%s:%d\n", char_ipaddr, LWIP_IANA_PORT_HTTPS);
    if(!connect_to_host(&ipaddr, &pcb)){
        printf("Failed to connect to https://%s:%d\n", char_ipaddr, LWIP_IANA_PORT_HTTPS);
                                        // TODO: Disconnect from network
        cyw43_arch_deinit();            // Deinit Pico W wireless hardware
        return;
    }
    printf("Connected to https://%s:%d\n", char_ipaddr, LWIP_IANA_PORT_HTTPS);


    init_lcd();

    // We update every 10 seconds
    weather_info = malloc(RESPONSE_SIZE);
    while(true)
    {
        weather_info_size = 0;
        send(ipaddr, char_ipaddr, pcb);

        // Await HTTP response
        printf("Awaiting response\n");
        while(weather_info_size == 0)
        {
            sleep_ms(PICOHTTPS_HTTP_RESPONSE_POLL_INTERVAL);
        }
        printf("Awaited response\n");
        render_temperature();
        render_time();

        sleep_ms(10000);
    }

    // Return
    printf("Exiting\n");
    return;
    //printf("Exited 😉 \n");
}

void render_time(void)
{
    const char* date_value;
    const char* date_tag = "Date: ";
    date_value = strstr(weather_info, date_tag);
    assert(date_value);
    date_value += strlen(date_tag);

    char date_no_time[] = "Sun, 22 Oct 2023";
    char date_time[] = "16:32:52 GMT";
    int date_no_time_len = strlen(date_no_time);
    int date_time_len = strlen(date_time);
    strncpy(date_no_time, date_value, date_no_time_len);
    strncpy(date_time, date_value + date_no_time_len + 1, date_time_len);

    // We first need to reset the LCD in the changed region
    GUI_DrawRectangle(20, 110, 480, 140 + 24, WHITE, DRAW_FULL, DOT_PIXEL_DFT);
    GUI_DisString_EN(20, 110, date_no_time, &Font24, LCD_BACKGROUND, BLACK);
    GUI_DisString_EN(20, 140, date_time, &Font24, LCD_BACKGROUND, BLACK);
}

void render_temperature(void)
{
    const char* temperature_json_value;
    const char* max_json_value;
    const char* temperature_json_tag = "\"temperature\":";
    const char* max_json_tag = "\"temperature_2m_max\":";

    temperature_json_value = strstr(weather_info, temperature_json_tag); // First occurence is the units
    assert(temperature_json_value);
    temperature_json_value = strstr(temperature_json_value + 1, temperature_json_tag); // Second occurence is the actual value
    temperature_json_value += strlen(temperature_json_tag);

    max_json_value = strstr(weather_info, max_json_tag); // First occurence is the units
    assert(max_json_value);
    max_json_value = strstr(max_json_value + 1, max_json_tag); // Second occurence is the actual value
    max_json_value += strlen(max_json_tag) + 1; // +1 for the [

    char temperature_value[5];
    strncpy(temperature_value, temperature_json_value, 4);
    temperature_value[4] = '\0';

    char max_value[5];
    strncpy(max_value, max_json_value, 4);
    max_value[4] = '\0';

    char temperature_string[] = "Temperature now: XXXX C";
    sprintf(temperature_string, "Temperature now: %s C", temperature_value);

    char max_string[] = "Temperature max: XXXX C";
    sprintf(max_string, "Temperature max: %s C", max_value);

    // We first need to reset the LCD in the changed region
    GUI_DrawRectangle(20, 50, 480, 80 + 24, WHITE, DRAW_FULL, DOT_PIXEL_DFT);
    GUI_DisString_EN(20, 50, temperature_string, &Font24, LCD_BACKGROUND, BLUE);
    GUI_DisString_EN(20, 80, max_string, &Font24, LCD_BACKGROUND, BLUE);
}

void init_lcd(void) {
	DEV_GPIO_Init();
	spi_init(SPI_PORT,4000000);
	gpio_set_function(LCD_CLK_PIN,GPIO_FUNC_SPI);
	gpio_set_function(LCD_MOSI_PIN,GPIO_FUNC_SPI);
	gpio_set_function(LCD_MISO_PIN,GPIO_FUNC_SPI);

	LCD_SCAN_DIR  lcd_scan_dir = SCAN_DIR_DFT;
	LCD_Init(lcd_scan_dir,800);
	TP_Init(lcd_scan_dir);
    GUI_Clear(WHITE);
    GUI_DisString_EN(20, 20, "SOS home assistant", &Font24, LCD_BACKGROUND, RED);
}

void send(ip_addr_t ipaddr, char* char_ipaddr, struct altcp_pcb* pcb) {
    // Send HTTP request to server
    printf("Sending request\n");
    if(!send_request(pcb)){
        printf("Failed to send request\n");
        altcp_free_config(              // Free connection configuration
            ((struct altcp_callback_arg*)(pcb->arg))->config
        );
        altcp_free_arg(                 // Free connection callback argument
            (struct altcp_callback_arg*)(pcb->arg)
        );
        altcp_free_pcb(pcb);            // Free connection PCB
                                        // TODO: Disconnect from network
        cyw43_arch_deinit();            // Deinit Pico W wireless hardware
        return;
    }
    printf("Request sent\n");
}



/* Functions ******************************************************************/

// Initialise standard I/O over USB
bool init_stdio(void){
    if(!stdio_usb_init()) return false;
    stdio_set_translate_crlf(&stdio_usb, false);
    return true;
}

// Initialise Pico W wireless hardware
bool init_cyw43(void){
    return !((bool)cyw43_arch_init_with_country(CYW43_COUNTRY_CZECH_REPUBLIC));
}

// Connect to wireless network
bool connect_to_network(void){
    cyw43_arch_enable_sta_mode();
    return !(
        (bool)cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID,
            WIFI_PASSWORD,
            CYW43_AUTH_WPA2_AES_PSK,
            PICOHTTPS_WIFI_TIMEOUT
        )
    );
}

// Resolve hostname
bool resolve_hostname(ip_addr_t* ipaddr){

    // Zero address
    ipaddr->addr = IPADDR_ANY;

    // Attempt resolution
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err = dns_gethostbyname(
        PICOHTTPS_HOSTNAME,
        ipaddr,
        callback_gethostbyname,
        ipaddr
    );
    cyw43_arch_lwip_end();
    if(lwip_err == ERR_INPROGRESS){

        // Await resolution
        //
        //  IP address will be made available shortly (by callback) upon DNS
        //  query response.
        //
        while(ipaddr->addr == IPADDR_ANY)
            sleep_ms(PICOHTTPS_RESOLVE_POLL_INTERVAL);
        if(ipaddr->addr != IPADDR_NONE)
            lwip_err = ERR_OK;

    }

    // Return
    return !((bool)lwip_err);

}

// Free TCP + TLS protocol control block
void altcp_free_pcb(struct altcp_pcb* pcb){
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err = altcp_close(pcb);         // Frees PCB
    cyw43_arch_lwip_end();
    while(lwip_err != ERR_OK)
        sleep_ms(PICOHTTPS_ALTCP_CONNECT_POLL_INTERVAL);
        cyw43_arch_lwip_begin();
        lwip_err = altcp_close(pcb);                // Frees PCB
        cyw43_arch_lwip_end();
}

// Free TCP + TLS connection configuration
void altcp_free_config(struct altcp_tls_config* config){
    cyw43_arch_lwip_begin();
    altcp_tls_free_config(config);
    cyw43_arch_lwip_end();
}

// Free TCP + TLS connection callback argument
void altcp_free_arg(struct altcp_callback_arg* arg){
    if(arg){
        free(arg);
    }
}

// Establish TCP + TLS connection with server
bool connect_to_host(ip_addr_t* ipaddr, struct altcp_pcb** pcb){

    const u8_t cert[] = TLS_ROOT_CERT;
    // Instantiate connection configuration
    cyw43_arch_lwip_begin();
    struct altcp_tls_config* config = altcp_tls_create_config_client(
        cert,
        LEN(cert)
    );
    cyw43_arch_lwip_end();
    if(!config) { printf("create_config_client failed\n"); return false; }

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
    if(!(*pcb)){
        altcp_free_config(config);
        return false;
    }

    // Configure common argument for connection callbacks
    //
    //  N.b. callback argument must be in scope in callbacks. As callbacks may
    //  fire after current function returns, cannot declare argument locally,
    //  but rather should allocate on the heap. Must then ensure allocated
    //  memory is subsequently freed.
    //
    struct altcp_callback_arg* arg = malloc(sizeof(*arg));
    if(!arg){
        altcp_free_pcb(*pcb);
        altcp_free_config(config);
        return false;
    }
    arg->config = config;
    arg->connected = false;
    cyw43_arch_lwip_begin();
    altcp_arg(*pcb, (void*)arg);
    cyw43_arch_lwip_end();

    // Configure connection fatal error callback
    cyw43_arch_lwip_begin();
    altcp_err(*pcb, callback_altcp_err);
    cyw43_arch_lwip_end();

    // Configure idle connection callback (and interval)
    cyw43_arch_lwip_begin();
    altcp_poll(
        *pcb,
        callback_altcp_poll,
        PICOHTTPS_ALTCP_IDLE_POLL_INTERVAL
    );
    cyw43_arch_lwip_end();

    printf("altcp_poll done\n");

    // Configure data acknowledge callback
    cyw43_arch_lwip_begin();
    altcp_sent(*pcb, callback_altcp_sent);
    cyw43_arch_lwip_end();

    printf("altcp_sent done\n");

    // Configure data reception callback
    cyw43_arch_lwip_begin();
    altcp_recv(*pcb, callback_altcp_recv);
    cyw43_arch_lwip_end();

    printf("altcp_recv done\n");

    // Send connection request (SYN)
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err = altcp_connect(
        *pcb,
        ipaddr,
        LWIP_IANA_PORT_HTTPS,
        callback_altcp_connect
    );
    cyw43_arch_lwip_end();


    // Connection request sent
    if(lwip_err == ERR_OK){

        // Await connection
        //
        //  Sucessful connection will be confirmed shortly in
        //  callback_altcp_connect.
        //
        while(!(arg->connected))
            sleep_ms(PICOHTTPS_ALTCP_CONNECT_POLL_INTERVAL);

    } else {

        // Free allocated resources
        altcp_free_pcb(*pcb);
        altcp_free_config(config);
        altcp_free_arg(arg);

    }

    // Return
    return !((bool)lwip_err);

}

// Send HTTP request
bool send_request(struct altcp_pcb* pcb){

    const char request[] = PICOHTTPS_REQUEST;

    // Check send buffer and queue length
    //
    //  Docs state that altcp_write() returns ERR_MEM on send buffer too small
    //  _or_ send queue too long. Could either check both before calling
    //  altcp_write, or just handle returned ERR_MEM — which is preferable?
    //
    //if(
    //  altcp_sndbuf(pcb) < (LEN(request) - 1)
    //  || altcp_sndqueuelen(pcb) > TCP_SND_QUEUELEN
    //) return -1;

    // Write to send buffer
    cyw43_arch_lwip_begin();
    lwip_err_t lwip_err = altcp_write(pcb, request, LEN(request) -1, 0);
    cyw43_arch_lwip_end();

    // Written to send buffer
    if(lwip_err == ERR_OK){

        // Output send buffer
        ((struct altcp_callback_arg*)(pcb->arg))->acknowledged = 0;
        cyw43_arch_lwip_begin();
        lwip_err = altcp_output(pcb);
        cyw43_arch_lwip_end();

        // Send buffer output
        if(lwip_err == ERR_OK){

            // Await acknowledgement
            while(
                !((struct altcp_callback_arg*)(pcb->arg))->acknowledged
            ) sleep_ms(PICOHTTPS_HTTP_RESPONSE_POLL_INTERVAL);
            if(
                ((struct altcp_callback_arg*)(pcb->arg))->acknowledged
                != (LEN(request) - 1)
            ) lwip_err = -1;

        }

    }

    // Return
    return !((bool)lwip_err);

}

// DNS response callback
void callback_gethostbyname(
    const char* name,
    const ip_addr_t* resolved,
    void* ipaddr
){
    if(resolved) *((ip_addr_t*)ipaddr) = *resolved;         // Successful resolution
    else ((ip_addr_t*)ipaddr)->addr = IPADDR_NONE;          // Failed resolution
}

// TCP + TLS connection error callback
void callback_altcp_err(void* arg, lwip_err_t err){

    // Print error code
    printf("Connection error [lwip_err_t err == %d]\n", err);

    // Free ALTCP TLS config
    if( ((struct altcp_callback_arg*)arg)->config )
        altcp_free_config( ((struct altcp_callback_arg*)arg)->config );

    // Free ALTCP callback argument
    altcp_free_arg((struct altcp_callback_arg*)arg);

}

// TCP + TLS connection idle callback
lwip_err_t callback_altcp_poll(void* arg, struct altcp_pcb* pcb){
    // Callback not currently used
    return ERR_OK;
}

// TCP + TLS data acknowledgement callback
lwip_err_t callback_altcp_sent(void* arg, struct altcp_pcb* pcb, u16_t len){
    ((struct altcp_callback_arg*)arg)->acknowledged = len;
    return ERR_OK;
}

// TCP + TLS data reception callback
lwip_err_t callback_altcp_recv(
    void* arg,
    struct altcp_pcb* pcb,
    struct pbuf* buf,
    lwip_err_t err
){

    // Store packet buffer at head of chain
    //
    //  Required to free entire packet buffer chain after processing.
    //
    struct pbuf* head = buf;

    switch(err){

        // No error receiving
        case ERR_OK:

            // Handle packet buffer chain
            //
            //  * buf->tot_len == buf->len — Last buf in chain
            //    * && buf->next == NULL — last buf in chain, no packets in queue
            //    * && buf->next != NULL — last buf in chain, more packets in queue
            //

            if(buf){

                // Print packet buffer
                u16_t i;
                while(buf->len != buf->tot_len){
                    for(i = 0; i < buf->len; i++) putchar(((char*)buf->payload)[i]);
                    buf = buf->next;
                }
                for(i = 0; i < buf->len; i++) putchar(((char*)buf->payload)[i]);
                assert(buf->next == NULL);

                // Advertise data reception
                altcp_recved(pcb, head->tot_len);

            }
            memcpy(weather_info, buf->payload, buf->tot_len);
            weather_info_size = buf->tot_len;

            // …fall-through…

        case ERR_ABRT:

            // Free buf
            pbuf_free(head);        // Free entire pbuf chain

            // Reset error
            err = ERR_OK;           // Only return ERR_ABRT when calling tcp_abort()

    }

    // Return error
    return err;

}

// TCP + TLS connection establishment callback
lwip_err_t callback_altcp_connect(
    void* arg,
    struct altcp_pcb* pcb,
    lwip_err_t err
){
    ((struct altcp_callback_arg*)arg)->connected = true;
    return ERR_OK;
}



