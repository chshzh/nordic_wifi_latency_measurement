/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/drivers/gpio.h>
#include <dk_buttons_and_leds.h>

#if IS_ENABLED(CONFIG_WIFI_READY_LIB)
#include <net/wifi_ready.h>
#endif

#include "wifi_utils.h"
#include "udp_utils.h"
#include "led_utils.h"
#include "raw_utils.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

/* Define a macro for the relevant network events */
#define L2_EVENT_MASK (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)
#define L3_EVENT_MASK NET_EVENT_IPV4_DHCP_BOUND

/* SoftAP specific events */
#define SOFTAP_EVENT_MASK (NET_EVENT_WIFI_AP_ENABLE_RESULT | \
                          NET_EVENT_WIFI_AP_STA_CONNECTED | \
                          NET_EVENT_WIFI_AP_STA_DISCONNECTED)

/* Button control for TX task */
static K_SEM_DEFINE(tx_start_sem, 0, 1);
static volatile bool tx_task_running = false;
static volatile bool tx_task_should_stop = false;

/* Global semaphore to wait for network connectivity */
K_SEM_DEFINE(network_connected, 0, 1);

/* Connection state tracking (custom enum to avoid conflicts) */
typedef enum {
    WIFI_CONN_STATE_DISCONNECTED,
    WIFI_CONN_STATE_CONNECTING,
    WIFI_CONN_STATE_CONNECTED,
    WIFI_CONN_STATE_FAILED
} wifi_connection_state_t;

static wifi_connection_state_t wifi_conn_state = WIFI_CONN_STATE_DISCONNECTED;
static int wifi_connection_retries = 0;
#define MAX_WIFI_RETRIES 3

#if IS_ENABLED(CONFIG_WIFI_READY_LIB)
/* WiFi ready support */
static K_SEM_DEFINE(wifi_ready_sem, 0, 1);
static bool wifi_ready_status = false;
#endif

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_MODE_SOFTAP)
/* SoftAP support */
static bool dhcp_server_started = false;
static K_MUTEX_DEFINE(softap_mutex);
static K_SEM_DEFINE(station_connected_sem, 0, 1);  /* Semaphore to wait for station connection */

struct softap_station {
    bool valid;
    struct wifi_ap_sta_info info;
    struct in_addr ip_addr;  /* Store station's IP address */
};

#define MAX_SOFTAP_STATIONS 4
static struct softap_station connected_stations[MAX_SOFTAP_STATIONS];
#endif /* CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_MODE_SOFTAP */

/* Declare the callback structures for Wi-Fi and network events */
static struct net_mgmt_event_callback wifi_mgmt_cb;
static struct net_mgmt_event_callback net_mgmt_cb;
/* Note: Supplicant callback removed - not available in this NCS version */

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_MODE_SOFTAP)
static struct net_mgmt_event_callback softap_mgmt_cb;
#endif /* CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_MODE_SOFTAP */

/* Define the boolean wifi_connected_signal */
static volatile bool wifi_connected_signal;

/* Helper function to get connection state string */
static const char *wifi_state_to_string(wifi_connection_state_t state)
{
    switch (state) {
    case WIFI_CONN_STATE_DISCONNECTED: return "DISCONNECTED";
    case WIFI_CONN_STATE_CONNECTING: return "CONNECTING";
    case WIFI_CONN_STATE_CONNECTED: return "CONNECTED";
    case WIFI_CONN_STATE_FAILED: return "FAILED";
    default: return "UNKNOWN";
    }
}

/* Note: Supplicant event handler removed - not available in this NCS version */

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_TX)
/* Button callback function for TX device */
static void button_handler(uint32_t button_state, uint32_t has_changed)
{
    if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
        LOG_INF("Button 1 pressed - restarting TX task");
        
        /* Stop current TX task if running */
        if (tx_task_running) {
            tx_task_should_stop = true;
            LOG_INF("Stopping current TX task...");
            
            /* Wait a bit for task to stop */
            k_sleep(K_MSEC(100));
        }
        
        /* Signal to start new TX task */
        k_sem_give(&tx_start_sem);
    }
}
#endif /* CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_TX */

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_MODE_SOFTAP)

static int get_station_ip_address(const uint8_t *mac, struct in_addr *ip_addr)
{
    struct net_if *iface;
    int station_count = 0;
    
    iface = net_if_get_first_wifi();
    if (!iface) {
        return -1;
    }
    
    /* Count existing stations to assign next sequential IP */
    k_mutex_lock(&softap_mutex, K_FOREVER);
    for (int i = 0; i < MAX_SOFTAP_STATIONS; i++) {
        if (connected_stations[i].valid && 
            connected_stations[i].ip_addr.s_addr != 0) {
            station_count++;
        }
    }
    k_mutex_unlock(&softap_mutex);
    
    /* Assign IP based on DHCP pool logic: 192.168.1.2, 192.168.1.3, etc. */
    /* DHCP server starts from 192.168.1.2, so assign sequentially */
    uint32_t base_ip = 0xC0A80102; /* 192.168.1.2 in network byte order (host order) */
    uint32_t assigned_ip = base_ip + station_count;
    
    ip_addr->s_addr = htonl(assigned_ip);
    
    LOG_DBG("Assigned IP for station: 192.168.1.%d (station count: %d)", 
            2 + station_count, station_count + 1);
    
    return 0;
}

static void handle_softap_enable_result(struct net_mgmt_event_callback *cb)
{
    const struct wifi_status *status = (const struct wifi_status *)cb->info;
    
    if (status->status) {
        LOG_ERR("SoftAP enable failed: %d", status->status);
    } else {
        LOG_INF("SoftAP enabled successfully");
        /* Signal network connectivity for SoftAP mode */
        k_sem_give(&network_connected);
    }
}

static void handle_station_connected(struct net_mgmt_event_callback *cb)
{
    const struct wifi_ap_sta_info *sta_info = (const struct wifi_ap_sta_info *)cb->info;
    int station_slot = -1;
    
    k_mutex_lock(&softap_mutex, K_FOREVER);
    
    /* Find empty slot for new station */
    for (int i = 0; i < MAX_SOFTAP_STATIONS; i++) {
        if (!connected_stations[i].valid) {
            connected_stations[i].valid = true;
            connected_stations[i].info = *sta_info;
            connected_stations[i].ip_addr.s_addr = 0; /* Initialize as unassigned */
            station_slot = i;
            break;
        }
    }
    
    k_mutex_unlock(&softap_mutex);
    
    uint8_t mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
            sta_info->mac[0], sta_info->mac[1], sta_info->mac[2],
            sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
    
    /* Try to get IP address after a short delay for DHCP assignment */
    k_sleep(K_MSEC(1000)); /* Wait 1 second for DHCP to assign IP */
    
    if (station_slot >= 0) {
        struct in_addr ip_addr;
        if (get_station_ip_address(sta_info->mac, &ip_addr) == 0) {
            k_mutex_lock(&softap_mutex, K_FOREVER);
            connected_stations[station_slot].ip_addr = ip_addr;
            k_mutex_unlock(&softap_mutex);
            
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &ip_addr, ip_str, sizeof(ip_str));
            LOG_INF("Station %s assigned IP: %s", mac_str, ip_str);
        } else {
            LOG_WRN("Could not determine IP address for station %s", mac_str);
        }
    }
    
    /* Signal that a station has connected - this will allow UDP RX task to start */
    k_sem_give(&station_connected_sem);
    LOG_INF("First station connected - UDP RX task can now start");
}

static void handle_station_disconnected(struct net_mgmt_event_callback *cb)
{
    const struct wifi_ap_sta_info *sta_info = (const struct wifi_ap_sta_info *)cb->info;
    char mac_str[18];
    char ip_str[INET_ADDRSTRLEN] = "Unknown";
    
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
            sta_info->mac[0], sta_info->mac[1], sta_info->mac[2],
            sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
    
    k_mutex_lock(&softap_mutex, K_FOREVER);
    
    /* Find and remove station from list, also get its IP address */
    for (int i = 0; i < MAX_SOFTAP_STATIONS; i++) {
        if (connected_stations[i].valid && 
            memcmp(connected_stations[i].info.mac, sta_info->mac, 6) == 0) {
            
            /* Get IP address before removing */
            if (connected_stations[i].ip_addr.s_addr != 0) {
                inet_ntop(AF_INET, &connected_stations[i].ip_addr, ip_str, sizeof(ip_str));
            }
            
            connected_stations[i].valid = false;
            connected_stations[i].ip_addr.s_addr = 0;
            break;
        }
    }
    
    k_mutex_unlock(&softap_mutex);
    
    LOG_INF("Station disconnected: MAC=%s, IP=%s", mac_str, ip_str);
    
    /* Check if any stations are still connected */
    k_mutex_lock(&softap_mutex, K_FOREVER);
    bool any_connected = false;
    for (int i = 0; i < MAX_SOFTAP_STATIONS; i++) {
        if (connected_stations[i].valid) {
            any_connected = true;
            break;
        }
    }
    k_mutex_unlock(&softap_mutex);
    
    if (!any_connected) {
        LOG_INF("No stations remaining connected to SoftAP");
        /* Note: UDP server continues running even if no stations connected */
        /* This allows for immediate packet reception when a new station connects */
    }
}

static void softap_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                     uint32_t mgmt_event, struct net_if *iface)
{
    switch (mgmt_event) {
    case NET_EVENT_WIFI_AP_ENABLE_RESULT:
        handle_softap_enable_result(cb);
        break;
    case NET_EVENT_WIFI_AP_STA_CONNECTED:
        handle_station_connected(cb);
        break;
    case NET_EVENT_WIFI_AP_STA_DISCONNECTED:
        handle_station_disconnected(cb);
        break;
    default:
        break;
    }
}

static int setup_dhcp_server(void)
{
    struct net_if *iface;
    struct in_addr pool_start;
    int ret;

    if (dhcp_server_started) {
        LOG_WRN("DHCP server already started");
        return 0;
    }

    iface = net_if_get_first_wifi();
    if (!iface) {
        LOG_ERR("Failed to get Wi-Fi interface");
        return -1;
    }

    /* Set DHCP pool start address */
    ret = inet_pton(AF_INET, "192.168.1.2", &pool_start);
    if (ret != 1) {
        LOG_ERR("Invalid DHCP pool start address");
        return -1;
    }

    ret = net_dhcpv4_server_start(iface, &pool_start);
    if (ret == -EALREADY) {
        LOG_INF("DHCP server already running");
        dhcp_server_started = true;
        return 0;
    } else if (ret < 0) {
        LOG_ERR("Failed to start DHCP server: %d", ret);
        return ret;
    }

    dhcp_server_started = true;
    LOG_INF("DHCP server started with pool starting at 192.168.1.2");
    return 0;
}

static int setup_softap_mode(void)
{
    int ret;

    LOG_INF("Setting up SoftAP mode");

    /* Set regulatory domain */
    ret = wifi_set_reg_domain();
    if (ret) {
        LOG_ERR("Failed to set regulatory domain: %d", ret);
        return ret;
    }

    /* Setup DHCP server */
    ret = setup_dhcp_server();
    if (ret) {
        LOG_ERR("Failed to setup DHCP server: %d", ret);
        return ret;
    }
    
    /* Setup SoftAP */
    ret = wifi_setup_softap(CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_SOFTAP_SSID,
                           CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_SOFTAP_PSK);
    if (ret) {
        LOG_ERR("Failed to setup SoftAP: %d", ret);
        return ret;
    }



    return 0;
}

#endif /* CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_MODE_SOFTAP */

#if IS_ENABLED(CONFIG_WIFI_READY_LIB)
void wifi_ready_callback(bool wifi_ready)
{
    LOG_INF("Wi-Fi ready: %s", wifi_ready ? "yes" : "no");
    wifi_ready_status = wifi_ready;
    k_sem_give(&wifi_ready_sem);
}

static int register_wifi_ready_cb(void)
{
    wifi_ready_callback_t cb = {
        .wifi_ready_cb = wifi_ready_callback
    };
    struct net_if *iface = net_if_get_first_wifi();

    if (!iface) {
        LOG_ERR("Failed to get Wi-Fi interface");
        return -1;
    }

    return register_wifi_ready_callback(cb, iface);
}
#endif

/* Enhanced WiFi management event handler for L2 events */
static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
                                   struct net_if *iface)
{
    switch (mgmt_event) {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        {
            const struct wifi_status *status = (const struct wifi_status *)cb->info;
            
            if (status->status == 0) {
                /* Connection successful */
                LOG_INF("WiFi L2 connection successful");
                wifi_conn_state = WIFI_CONN_STATE_CONNECTED;
                wifi_connection_retries = 0;  /* Reset retry counter */
                
                /* Print detailed WiFi status when connected */
                wifi_print_status();
                wifi_connected_signal = true;
                
                LOG_INF("Connection state: %s", wifi_state_to_string(wifi_conn_state));
            } else {
                /* Connection failed */
                wifi_conn_state = WIFI_CONN_STATE_FAILED;
                wifi_connection_retries++;
                
                LOG_ERR("WiFi connection failed: status=%d, retries=%d/%d", 
                       status->status, wifi_connection_retries, MAX_WIFI_RETRIES);
                
                /* Decode common error codes */
                switch (status->status) {
                case 1: LOG_ERR("  Reason: Generic failure"); break;
                case 2: LOG_ERR("  Reason: Authentication timeout"); break;
                case 3: LOG_ERR("  Reason: Authentication failed"); break;
                case 15: LOG_ERR("  Reason: AP not found"); break;
                case 16: LOG_ERR("  Reason: Association timeout"); break;
                default: LOG_ERR("  Reason: Unknown error code %d", status->status); break;
                }
                
                /* Implement retry logic */
                if (wifi_connection_retries < MAX_WIFI_RETRIES) {
                    LOG_INF("Retrying connection in 5 seconds...");
                    wifi_conn_state = WIFI_CONN_STATE_DISCONNECTED;
                    /* Note: Actual retry would need to be implemented in main loop */
                } else {
                    LOG_ERR("Max retries reached. Connection failed permanently.");
                }
                
                wifi_connected_signal = false;
                LOG_INF("Connection state: %s", wifi_state_to_string(wifi_conn_state));
            }
        }
        break;
        
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        {
            const struct wifi_status *status = (const struct wifi_status *)cb->info;
            
            if (wifi_connected_signal == false) {
                LOG_INF("Waiting for WiFi to be connected");
                wifi_conn_state = WIFI_CONN_STATE_CONNECTING;
            } else {
                LOG_INF("WiFi disconnected: status=%d", status ? status->status : -1);
                wifi_conn_state = WIFI_CONN_STATE_DISCONNECTED;
                wifi_connected_signal = false;
            }
            
            LOG_INF("Connection state: %s", wifi_state_to_string(wifi_conn_state));
        }
        break;
        
    default:
        LOG_DBG("Unhandled WiFi event: 0x%08X", mgmt_event);
        break;
    }
}

/* Enhanced network management event handler for L3 events */
static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
                                  struct net_if *iface)
{
    if ((mgmt_event & L3_EVENT_MASK) != mgmt_event) {
        return;
    }

    switch (mgmt_event) {
    case NET_EVENT_IPV4_DHCP_BOUND:
        LOG_INF("Network DHCP bound - L3 connectivity established");
        /* Print IP address information */
        wifi_print_dhcp_ip(cb);
        
        /* Update connection state if we're in WiFi connected state */
        if (wifi_conn_state == WIFI_CONN_STATE_CONNECTED) {
            LOG_INF("Full network stack ready (L2 + L3)");
        }
        
        /* Signal network connectivity */
        k_sem_give(&network_connected);
        break;
        
    default:
        LOG_DBG("Unhandled network event: 0x%08X", mgmt_event);
        break;
    }
}

static int init_network_events(void)
{
    LOG_INF("Initializing network event handlers");
    
    /* Note: Supplicant events not available in this NCS version */

    /* Initialize and add the callback function for WiFi events (L2) */
    net_mgmt_init_event_callback(&wifi_mgmt_cb, wifi_mgmt_event_handler, L2_EVENT_MASK);
    net_mgmt_add_event_callback(&wifi_mgmt_cb);
    LOG_DBG("WiFi L2 event handler registered");

    /* Initialize and add the callback function for network events (L3) */
    net_mgmt_init_event_callback(&net_mgmt_cb, net_mgmt_event_handler, L3_EVENT_MASK);
    net_mgmt_add_event_callback(&net_mgmt_cb);
    LOG_DBG("Network L3 event handler registered");

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_MODE_SOFTAP)
    /* Initialize SoftAP event callbacks */
    net_mgmt_init_event_callback(&softap_mgmt_cb, softap_mgmt_event_handler, SOFTAP_EVENT_MASK);
    net_mgmt_add_event_callback(&softap_mgmt_cb);
    LOG_DBG("SoftAP event handler registered");
#endif
    
    LOG_INF("All network event handlers initialized successfully");
    return 0;
}

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_TX) || \
    (IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_RX) && \
     IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_MODE_STA))
/* Enhanced connection function with better debugging */
static int connect_to_network(void)
{
    int ret;

    LOG_INF("Starting network connection process");
    LOG_INF("Connection state: %s", wifi_state_to_string(wifi_conn_state));

    /* Set connecting state */
    wifi_conn_state = WIFI_CONN_STATE_CONNECTING;

    LOG_INF("Initiating connection to network");
    ret = conn_mgr_all_if_connect(true);
    if (ret) {
        LOG_ERR("Failed to initiate network connection: %d", ret);
        wifi_conn_state = WIFI_CONN_STATE_FAILED;
        return ret;
    }
    
    LOG_INF("Connection attempt initiated successfully");
    LOG_INF("Connection state: %s", wifi_state_to_string(wifi_conn_state));
    
    return 0;
}

/* Helper function for connection with timeout and retry */
static int connect_with_retry(int timeout_sec)
{
    int ret;
    int attempts = 0;
    
    while (attempts < MAX_WIFI_RETRIES) {
        attempts++;
        LOG_INF("Connection attempt %d/%d", attempts, MAX_WIFI_RETRIES);
        
        ret = connect_to_network();
        if (ret) {
            LOG_ERR("Connection initiation failed on attempt %d", attempts);
            if (attempts < MAX_WIFI_RETRIES) {
                LOG_INF("Waiting 5 seconds before retry...");
                k_sleep(K_SECONDS(5));
                continue;
            } else {
                LOG_ERR("All connection attempts failed");
                return ret;
            }
        }
        
        /* Wait for connection result */
        ret = k_sem_take(&network_connected, K_SECONDS(timeout_sec));
        if (ret == 0) {
            LOG_INF("Network connected successfully on attempt %d", attempts);
            return 0;
        } else {
            LOG_ERR("Connection timeout on attempt %d", attempts);
            if (attempts < MAX_WIFI_RETRIES) {
                LOG_INF("Retrying connection...");
            }
        }
    }
    
    LOG_ERR("Failed to connect after %d attempts", MAX_WIFI_RETRIES);
    return -1;
}
#endif

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_TX) && \
    IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_UDP)
static void udp_tx_session(void)
{
    int ret;
    int udp_socket;
    struct sockaddr_in server_addr;
    uint32_t packet_count = 0;
    int64_t start_time;
    uint32_t test_duration = CONFIG_WIFI_LATENCY_TEST_DURATION_MS;
    uint32_t packet_interval = CONFIG_WIFI_LATENCY_TEST_INTERVAL_MS;

    LOG_INF("Starting UDP TX session");

    /* Mark task as running */
    tx_task_running = true;
    tx_task_should_stop = false;

    /* Create UDP socket */
    ret = udp_client_init(&udp_socket, &server_addr, 
                         CONFIG_WIFI_LATENCY_TEST_TARGET_IP,
                         CONFIG_WIFI_LATENCY_TEST_SOCKET_PORT);
    if (ret) {
        LOG_ERR("Failed to initialize UDP client: %d", ret);
        tx_task_running = false;
        return;
    }

    start_time = k_uptime_get();

    /* Main transmission loop */
    while ((k_uptime_get() - start_time) < test_duration && !tx_task_should_stop) {
        char payload[64];
        int64_t current_time = k_uptime_get();

        /* Prepare payload with timestamp and packet count */
        snprintf(payload, sizeof(payload), "Packet_%u_Time_%lld", 
                packet_count, current_time);

        /* Trigger LED before transmission */
        led_trigger_tx();

        /* Send UDP packet */
        ret = udp_send(udp_socket, &server_addr, payload, strlen(payload));
        if (ret < 0) {
            LOG_ERR("Failed to send UDP packet: %d", ret);
        } else {
            LOG_INF("Sent: UDP packet %u at %lld ms", packet_count, current_time);
            packet_count++;
        }

        /* Wait for next transmission with shorter intervals to check for stop signal */
        for (int i = 0; i < packet_interval/10 && !tx_task_should_stop; i++) {
            k_sleep(K_MSEC(10));
        }
    }

    if (tx_task_should_stop) {
        LOG_INF("TX session stopped by button. Sent %u packets", packet_count);
    } else {
        LOG_INF("TX session completed. Sent %u packets", packet_count);
    }
    
    udp_client_cleanup(udp_socket);
    tx_task_running = false;
}

static void udp_tx_task(void)
{
    LOG_INF("TX device ready. Press Button 1 to start/restart packet transmission");
    
    /* Start first transmission session */
    udp_tx_session();
    
    /* Wait for button presses to restart transmission */
    while (1) {
        /* Wait for button press to restart */
        k_sem_take(&tx_start_sem, K_FOREVER);
        
        LOG_INF("Button 1 pressed - starting new TX session");
        udp_tx_session();
    }
}
#endif /* CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_TX && CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_UDP */

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_RX)
static void udp_rx_task(void)
{
    int ret;
    int udp_socket;
    uint32_t packet_count = 0;

    /* Create UDP socket for receiving */
    ret = udp_server_init(&udp_socket, CONFIG_WIFI_LATENCY_TEST_SOCKET_PORT);
    if (ret) {
        LOG_ERR("Failed to initialize UDP server: %d", ret);
        return;
    }

    LOG_INF("UDP server listening on port %d", CONFIG_WIFI_LATENCY_TEST_SOCKET_PORT);

    /* Main reception loop */
    while (1) {
        char buffer[256];
        int64_t current_time = k_uptime_get();

        ret = udp_receive(udp_socket, buffer, sizeof(buffer));
        if (ret > 0) {
            /* Trigger LED when packet received */
            led_trigger_rx();
            
            buffer[ret] = '\0'; /* Null-terminate */
            LOG_INF("Received: %s at %lld ms", buffer, current_time);
            packet_count++;
        } else if (ret < 0) {
            LOG_ERR("Failed to receive UDP packet: %d", ret);
            k_sleep(K_MSEC(100));
        }
    }

    LOG_INF("RX task completed. Received %u packets", packet_count);
    udp_server_cleanup(udp_socket);
}
#endif /* CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_RX */

int main(void)
{
    int ret;

    LOG_INF("Starting Wi-Fi Packet Latency Test Application");
#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_UDP)
    LOG_INF("Transmission mode: UDP packets");
#endif

    /* Initialize LED GPIO for timing measurements */
    ret = led_init();
    if (ret) {
        LOG_ERR("Failed to initialize LEDs: %d", ret);
        return ret;
    }

    /* Initialize network event handling */
    ret = init_network_events();
    if (ret) {
        LOG_ERR("Failed to initialize network events: %d", ret);
        return ret;
    }

#if IS_ENABLED(CONFIG_WIFI_READY_LIB)
    /* Register WiFi ready callback */
    ret = register_wifi_ready_cb();
    if (ret) {
        LOG_ERR("Failed to register WiFi ready callback: %d", ret);
        return ret;
    }

    /* Wait for WiFi to be ready */
    LOG_INF("Waiting for Wi-Fi to be ready (using WiFi Ready Library)...");
    ret = k_sem_take(&wifi_ready_sem, K_SECONDS(30));
    if (ret) {
        LOG_ERR("Timeout waiting for Wi-Fi ready");
        return ret;
    }

    if (!wifi_ready_status) {
        LOG_ERR("Wi-Fi not ready");
        return -1;
    }
#else
    LOG_INF("WiFi Ready Library not enabled - proceeding without explicit ready check");
#endif

    /* Additional WiFi subsystem verification */
    struct net_if *iface = net_if_get_first_wifi();
    if (!iface) {
        LOG_ERR("No WiFi interface found");
        return -1;
    }
    
    LOG_INF("WiFi interface found: %p", iface);
    LOG_INF("WiFi subsystem initialization complete");

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_TX)
    /* TX device */
    LOG_INF("Device role: TX");

    /* Use enhanced connection with retry logic */ 
    ret = connect_with_retry(60);
    if (ret) {
        LOG_ERR("Failed to establish network connection after retries");
        return ret;
    }

    /* Initialize buttons for TX control */
    ret = dk_buttons_init(button_handler);
    if (ret) {
        LOG_ERR("Failed to initialize buttons: %d", ret);
        return ret;
    }

    LOG_INF("Network connected successfully, starting TX task");
    LOG_INF("Button 1: Start/restart packet transmission");
#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_UDP)
    udp_tx_task();
#endif

#elif IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_RX) 
#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_MODE_SOFTAP)
    /* RX device in SoftAP mode */
    LOG_INF("Device role: RX (SoftAP mode)");

    ret = setup_softap_mode();
    if (ret) {
        LOG_ERR("Failed to setup SoftAP mode: %d", ret);
        return ret;
    }

    /* Print detailed WiFi status when connected */
    wifi_print_status();

    LOG_INF("SoftAP setup complete, waiting for station to connect...");
    LOG_INF("SSID: %s", CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_SOFTAP_SSID);
    LOG_INF("Password: %s", CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_SOFTAP_PSK);
    LOG_INF("UDP server will start once a station connects");
    
    /* Wait for a station to connect before starting UDP RX */
    ret = k_sem_take(&station_connected_sem, K_FOREVER);
    if (ret) {
        LOG_ERR("Error waiting for station connection: %d", ret);
        return ret;
    }
    
    LOG_INF("Station connected! Starting RX server...");


#elif IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_MODE_STA)
    /* RX device in station mode */
    LOG_INF("Device role: RX (Station mode)");
    
    /* Use enhanced connection with retry logic */
    ret = connect_with_retry(60);
    if (ret) {
        LOG_ERR("Failed to establish network connection after retries");
        return ret;
    }

    LOG_INF("Network connected successfully, starting RX task");
#endif /* CONFIG_WIFI_LATENCY_TEST_RX_DEVICE_MODE_SOFTAP */

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_UDP)
    udp_rx_task();
#endif

#else
    LOG_ERR("No valid device role configured");
    return -1;
#endif

    /* Should not reach here */
    return 0;
} 