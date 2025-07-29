/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <supp_events.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/drivers/gpio.h>
#include <dk_buttons_and_leds.h>

#include "wifi_utils.h"
#include "udp_utils.h"
#include "led_utils.h"
#include "raw_utils.h"
#include "net_event_mgmt.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

/* External semaphores from net_event_mgmt.c */
extern struct k_sem iface_up_sem;
extern struct k_sem wpa_supplicant_ready_sem;
extern struct k_sem ipv4_dhcp_bond_sem;

/* External semaphore from net_event_mgmt.c for SoftAP mode */
#if IS_ENABLED(CONFIG_UDP_RX_DEV_MODE_SOFTAP)
extern struct k_sem station_connected_sem;
extern bool dhcp_server_started;
#endif
/* Function declarations */

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_TX)
/* Button control for TX task */
K_SEM_DEFINE(tx_start_sem, 0, 1);
volatile bool tx_task_running = false;
volatile bool tx_task_should_stop = false;

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

#if IS_ENABLED(CONFIG_UDP_RX_DEV_MODE_SOFTAP)
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
    ret = wifi_setup_softap(CONFIG_UDP_RX_DEV_MODE_SOFTAP_SSID,
                           CONFIG_UDP_RX_DEV_MODE_SOFTAP_PSK);
    if (ret) {
        LOG_ERR("Failed to setup SoftAP: %d", ret);
        return ret;
    }



    return 0;
}
#endif /* CONFIG_UDP_RX_DEV_MODE_SOFTAP */

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_TX) && \
    IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_RAW)
static void raw_tx_session(void)
{
    int ret;
    uint32_t packet_count = 0;
    int64_t start_time;
    uint32_t test_duration = CONFIG_WIFI_LATENCY_TEST_DURATION_MS;
    uint32_t packet_interval = CONFIG_WIFI_LATENCY_TEST_INTERVAL_MS;
    LOG_INF("Starting Raw TX session"); 
    /* Mark task as running */
    tx_task_running = true;
    tx_task_should_stop = false;  

    ret = raw_tx_socket_init();

    if (ret < 0) {
        LOG_ERR("Failed to initialize raw TX socket");
        tx_task_running = false;
        return;
    }

    start_time = k_uptime_get();    
    /* Main transmission loop */
    while ((k_uptime_get() - start_time) < test_duration && !tx_task_should_stop) {

        /* Prepare and trigger LED before transmission */
        led_trigger_tx();       
        /* Send raw packet */
        ret = raw_tx_send_packet(packet_count);
        if (ret < 0) {
            LOG_ERR("Failed to send raw packet: %d", ret);
            break;  /* Exit loop on error */
        } else {
            LOG_INF("Sent: Raw packet %u at %lld ms", packet_count, k_uptime_get());
        }
    
        packet_count++; 
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
    /* Cleanup raw socket */
    raw_tx_cleanup();
    tx_task_running = false;
    LOG_INF("Raw TX session completed");
    /* Reset task state */
    tx_task_should_stop = false;
    tx_task_running = false;
    LOG_INF("Raw TX task finished, Press Button 1 to restart packet transmission");
}

static void raw_tx_task(void)
{
    LOG_INF("TX device ready. Press Button 1 to start/restart packet transmission");
    
    /* Start first transmission session */
    raw_tx_session();
    
    /* Wait for button presses to restart transmission */
    while (1) {
        /* Wait for button press to restart */
        k_sem_take(&tx_start_sem, K_FOREVER);
        
        LOG_INF("Button 1 pressed - starting new TX session");
        raw_tx_session();
    }
}
#endif /* CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_TX && CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_RAW */

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
                         CONFIG_UDP_TX_DEV_TARGET_IP,
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
    LOG_INF("UDP TX task finished, Press Button 1 to start/restart packet transmission");
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

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_UDP) && IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_RX)
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
#elif IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_RAW)
    LOG_INF("Transmission mode: Raw IEEE 802.11 packets");
#endif

    /* Initialize LED GPIO for timing measurements */
    ret = led_init();
    if (ret) {
        LOG_ERR("Failed to initialize LEDs: %d", ret);
        return ret;
    }

    ret = init_network_events();
    if (ret) {
        LOG_ERR("Failed to initialize network events: %d", ret);
        return ret;
    }
    k_sem_take(&wpa_supplicant_ready_sem, K_FOREVER);
    LOG_INF("WPA Supplicant is ready!");

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_TX)
    /* TX device */
    LOG_INF("Device role: TX");

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_RAW)    
    /* Raw packet transmission */
    ret = raw_tx_init();
    if (ret) {
        LOG_ERR("Failed to initialize raw TX: %d", ret);
        return ret;
    }

    /* Wait for interface to become operational */
	ret = k_sem_take(&iface_up_sem, K_SECONDS(30));
	if (ret) {
		LOG_ERR("Timeout waiting for interface to become operational");
		return ret;
	}

    /* Initialize buttons for TX control */
    ret = dk_buttons_init(button_handler);
    if (ret) {
        LOG_ERR("Failed to initialize buttons: %d", ret);
        return ret;
    }

    LOG_INF("Raw packet TX initialized, starting TX task");
    LOG_INF("Button 1: Start/restart raw packet transmission");
    raw_tx_task();

#elif IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_UDP)
    /* UDP packet transmission */
    ret = conn_mgr_all_if_connect(true);
    if (ret) {
        LOG_ERR("Failed to initiate network connection: %d", ret);
        return ret;
    }
    LOG_INF("Network connection initiated, waiting for IPv4 DHCP bond...");
    k_sem_take(&ipv4_dhcp_bond_sem, K_FOREVER);
    
    /* Initialize buttons for TX control */
    ret = dk_buttons_init(button_handler);
    if (ret) {
        LOG_ERR("Failed to initialize buttons: %d", ret);
        return ret;
    }

    LOG_INF("Network connected successfully, starting TX task");
    LOG_INF("Button 1: Start/restart packet transmission");
    udp_tx_task();
#endif

#elif IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_RX) 

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_RAW)
    /* Raw packet reception */
    
#if IS_ENABLED(CONFIG_RAW_RX_DEV_MODE_MONITOR)
    /* Monitor mode */
    LOG_INF("Device role: RX (Monitor mode)");
    ret = raw_rx_dev_monitor_init();
    if (ret) {
        LOG_ERR("Failed to initialize monitor mode: %d", ret);
        return ret;
    }
    /* Wait for interface to become operational */
	ret = k_sem_take(&iface_up_sem,  K_SECONDS(30));
	if (ret) {
		LOG_ERR("Timeout waiting for interface to become operational");
		return ret;
	}
    raw_rx_dev_monitor_task();

#elif IS_ENABLED(CONFIG_RAW_RX_DEV_MODE_PROMISCUOUS)
    /* Promiscuous mode */
    LOG_INF("Device role: RX (Promiscuous mode)");
    
    /* Need to connect to AP first for promiscuous mode */
    ret = connect_with_retry(60);
    if (ret) {
        LOG_ERR("Failed to establish network connection for promiscuous mode");
        return ret;
    }
    
    ret = raw_rx_dev_promiscuous_init();
    if (ret) {
        LOG_ERR("Failed to initialize promiscuous mode: %d", ret);
        return ret;
    }
    raw_rx_dev_promiscuous_task();
#endif

#elif IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_UDP)
    /* UDP packet reception */

#if IS_ENABLED(CONFIG_UDP_RX_DEV_MODE_SOFTAP)
    /* RX device in SoftAP mode */
    LOG_INF("Device role: RX (SoftAP mode)");

    ret = setup_softap_mode();
    if (ret) {
        LOG_ERR("Failed to setup SoftAP mode: %d", ret);
        return ret;
    }
    ret = k_sem_take(&ipv4_dhcp_bond_sem, K_FOREVER);
    /* Print detailed WiFi status when connected */
    wifi_print_status();

    LOG_INF("SoftAP setup complete, waiting for station to connect...");
    LOG_INF("SSID: %s", CONFIG_UDP_RX_DEV_MODE_SOFTAP_SSID);
    LOG_INF("Password: %s", CONFIG_UDP_RX_DEV_MODE_SOFTAP_PSK);
    LOG_INF("UDP server will start once a station connects");
    
    /* Wait for a station to connect before starting UDP RX */
    ret = k_sem_take(&station_connected_sem, K_FOREVER);
    if (ret) {
        LOG_ERR("Error waiting for station connection: %d", ret);
        return ret;
    }
    
    LOG_INF("Station connected! Starting RX server...");

#elif IS_ENABLED(CONFIG_UDP_RX_DEV_MODE_STA)
    /* RX device in station mode */
    LOG_INF("Device role: RX (Station mode)");
    ret = conn_mgr_all_if_connect(true);
    if (ret) {
        LOG_ERR("Failed to initiate network connection: %d", ret);
        return ret;
    }
    LOG_INF("Network connection initiated, waiting for IPv4 DHCP bond...");
    k_sem_take(&ipv4_dhcp_bond_sem, K_FOREVER);

    LOG_INF("Network connected successfully, starting RX task");
#endif /* CONFIG_UDP_RX_DEV_MODE_SOFTAP */

    udp_rx_task();
#endif

#else
    LOG_ERR("No valid device role configured");
    return -1;
#endif

    /* Should not reach here */
    return 0;
} 