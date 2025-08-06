/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/random/random.h>
#include <zephyr/posix/arpa/inet.h>
#include <string.h>

#include "raw_utils.h"
#include "wifi_utils.h"
#include "led_utils.h"
#include "net_event_mgmt.h"

LOG_MODULE_REGISTER(raw_utils, CONFIG_LOG_DEFAULT_LEVEL);

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_RAW)
#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_TX)
/* Constants */
#define NRF_WIFI_MAGIC_NUM_RAWTX        0x12345678
#define IEEE80211_SEQ_CTRL_SEQ_NUM_MASK 0xFFF0
#define IEEE80211_SEQ_NUMBER_INC        BIT(4)

/* Global variables */
static int raw_sockfd = -1;
static struct sockaddr_ll sa;

/* Test beacon frame template with identifiable payload */
static struct beacon_frame test_beacon_frame = {
	.frame_control = htons(0x8000), /* Beacon frame */
	.duration = 0x0000,
	.da = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, /* Broadcast */
	.sa = {0xA0, 0x69, 0x60, 0xE3, 0x52, 0x15}, /* Our MAC */
	.bssid = {0xA0, 0x69, 0x60, 0xE3, 0x52, 0x15},
	.seq_ctrl = 0x0001,
	/* SSID: WIFI_LATENCY_TEST */
	.payload = {0X0C, 0XA2, 0X28, 0X00, 0X00, 0X00, 0X00, 0X00, 0X64, 0X00, 0X11, 0X04,

		    /* SSID Length*/
		    0X00, 0X11,
		    /* SSID: WIFI_LATENCY_TEST */
		    0x57, 0x49, 0x46, 0x49, 0x5F, 0x4C, 0x41, 0x54, 0x45, 0x4E, 0x43, 0x59, 0x5F,
		    0x54, 0x45, 0x53, 0x54,

		    0X01, 0X08, 0X82, 0X84, 0X8B, 0X96, 0X0C, 0X12, 0X18, 0X24, 0X03, 0X01, 0X06,
		    0X05, 0X04, 0X00, 0X02, 0X00, 0X00, 0X2A, 0X01, 0X04, 0X32, 0X04, 0X30, 0X48,
		    0X60, 0X6C, 0X30, 0X14, 0X01, 0X00, 0X00, 0X0F, 0XAC, 0X04, 0X01, 0X00, 0X00,
		    0X0F, 0XAC, 0X04, 0X01, 0X00, 0X00, 0X0F, 0XAC, 0X02, 0X0C, 0X00, 0X3B, 0X02,
		    0X51, 0X00, 0X2D, 0X1A, 0X0C, 0X00, 0X17, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00,
		    0X00, 0X00, 0X00, 0X2C, 0X01, 0X01, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
		    0X00, 0X00, 0X3D, 0X16, 0X06, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
		    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X7F, 0X08, 0X04,
		    0X00, 0X00, 0X02, 0X00, 0X00, 0X00, 0X40, 0XFF, 0X1A, 0X23, 0X01, 0X78, 0X10,
		    0X1A, 0X00, 0X00, 0X00, 0X20, 0X0E, 0X09, 0X00, 0X09, 0X80, 0X04, 0X01, 0XC4,
		    0X00, 0XFA, 0XFF, 0XFA, 0XFF, 0X61, 0X1C, 0XC7, 0X71, 0XFF, 0X07, 0X24, 0XF0,
		    0X3F, 0X00, 0X81, 0XFC, 0XFF, 0XDD, 0X18, 0X00, 0X50, 0XF2, 0X02, 0X01, 0X01,
		    0X01, 0X00, 0X03, 0XA4, 0X00, 0X00, 0X27, 0XA4, 0X00, 0X00, 0X42, 0X43, 0X5E,
		    0X00, 0X62, 0X32, 0X2F, 0X00}};

/* Setup raw packet socket */
int raw_tx_socket_init(void)
{
	struct net_if *iface;
	int ret;

	raw_sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (raw_sockfd < 0) {
		LOG_ERR("Unable to create socket: %d", errno);
		return -errno;
	}

	iface = net_if_get_first_wifi();
	if (!iface) {
		LOG_ERR("Failed to get Wi-Fi interface");
		close(raw_sockfd);
		raw_sockfd = -1;
		return -ENODEV;
	}

	sa.sll_family = AF_PACKET;
	sa.sll_ifindex = net_if_get_by_iface(iface);

	ret = bind(raw_sockfd, (struct sockaddr *)&sa, sizeof(struct sockaddr_ll));
	if (ret < 0) {
		LOG_ERR("Unable to bind socket: %s", strerror(errno));
		close(raw_sockfd);
		raw_sockfd = -1;
		return -errno;
	}

	LOG_INF("Raw packet socket created and bound");
	return 0;
}

static void fill_raw_tx_pkt_hdr(struct raw_tx_pkt_header *raw_tx_pkt)
{
	raw_tx_pkt->magic_num = NRF_WIFI_MAGIC_NUM_RAWTX;
	raw_tx_pkt->data_rate = CONFIG_RAW_TX_DEV_RATE_VALUE;
	raw_tx_pkt->packet_length = sizeof(test_beacon_frame);
	raw_tx_pkt->tx_mode = CONFIG_RAW_TX_DEV_RATE_FLAGS;
	raw_tx_pkt->queue = CONFIG_RAW_TX_DEV_QUEUE_NUM;
	raw_tx_pkt->raw_tx_flag = 0; /* Reserved for driver */
}

static void increment_seq_control(void)
{
	test_beacon_frame.seq_ctrl = (test_beacon_frame.seq_ctrl + IEEE80211_SEQ_NUMBER_INC) &
				     IEEE80211_SEQ_CTRL_SEQ_NUM_MASK;
	if (test_beacon_frame.seq_ctrl > IEEE80211_SEQ_CTRL_SEQ_NUM_MASK) {
		test_beacon_frame.seq_ctrl = 0x0010;
	}
}

int raw_tx_init(void)
{
	int ret;

	/* Set Wi-Fi mode to station */
	ret = wifi_set_mode(WIFI_STA_MODE);
	if (ret) {
		LOG_ERR("Failed to set Wi-Fi mode: %d", ret);
		return ret;
	}

#ifdef CONFIG_RAW_TX_DEV_INJECTION_ENABLE
	ret = wifi_set_tx_injection_mode();
	if (ret) {
		LOG_ERR("Failed to enable TX injection mode: %d", ret);
		return ret;
	}
#endif

#ifdef CONFIG_RAW_TX_DEV_MODE_NON_CONNECTED
	ret = wifi_set_channel(CONFIG_RAW_TX_DEV_CHANNEL);
	if (ret) {
		LOG_ERR("Failed to set Wi-Fi channel: %d", ret);
		return ret;
	}
#endif

	LOG_INF("Raw TX initialization complete");
	return 0;
}

int raw_tx_send_packet(uint32_t packet_num)
{
	struct raw_tx_pkt_header packet_hdr;
	char *test_frame;
	size_t buf_length;
	int ret;

	if (raw_sockfd < 0) {
		LOG_ERR("Raw socket not initialized");
		return -ENOTCONN;
	}

	/* Fill the raw packet header */
	fill_raw_tx_pkt_hdr(&packet_hdr);

	/* Allocate buffer for header + frame */
	buf_length = sizeof(struct raw_tx_pkt_header) + sizeof(test_beacon_frame);
	test_frame = k_malloc(buf_length);
	if (!test_frame) {
		LOG_ERR("Failed to allocate transmission buffer");
		led_trigger_rx();
		return -ENOMEM;
	}

	/* Copy header and frame to buffer */
	memcpy(test_frame, &packet_hdr, sizeof(struct raw_tx_pkt_header));
	/* Copy the test beacon frame */
	memcpy(test_frame + sizeof(struct raw_tx_pkt_header), &test_beacon_frame,
	       sizeof(test_beacon_frame));

	/* Send the packet */
	ret = sendto(raw_sockfd, test_frame, buf_length, 0, (struct sockaddr *)&sa, sizeof(sa));
	if (ret < 0) {
		LOG_ERR("Failed to send raw packet: %s", strerror(errno));
		k_free(test_frame);
		return -errno;
	}
	/* Increment sequence control for next packet */
	increment_seq_control();

	k_free(test_frame);
	return 0;
}

void raw_tx_cleanup(void)
{
	if (raw_sockfd >= 0) {
		close(raw_sockfd);
		raw_sockfd = -1;
	}
	LOG_INF("Raw TX cleanup complete");
}
#endif /* CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_RAW && CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_TX */

#if IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_RX)
static raw_packet_stats_t rx_stats = {0};
#define RAW_PKT_HDR_SIZE    6
#define TEST_SSID_SIGNATURE "WIFI_LATENCY_TEST"

/* Function to check if a beacon frame is from the raw_tx_packet project */
static bool is_raw_tx_packet_beacon(unsigned char *packet, int packet_len)
{
	/* Basic 802.11 frame structure:
	 * Frame Control (2) + Duration (2) + DA (6) + SA (6) + BSSID (6) + Seq Ctrl (2) = 24 bytes
	 * Then comes the beacon frame body which includes timestamp (8), beacon interval (2),
	 * capability info (2), then information elements starting with SSID
	 */

	const char target_ssid[] = "WIFI_LATENCY_TEST";
	const int ssid_len = strlen(target_ssid);

	/* Minimum length check: 802.11 header (24) + beacon fixed fields (12) + SSID IE header
	 * (2)*/
	if (packet_len < 38) {
		return false;
	}

	/* Skip to beacon frame body (after 24-byte 802.11 header) */
	unsigned char *beacon_body = packet + 24;

	/* Skip timestamp (8), beacon interval (2), capability info (2) = 12 bytes */
	unsigned char *ie_start = beacon_body + 12;
	int remaining_len = packet_len - 36; /* 24 (header) + 12 (fixed fields) */

	/* Look for SSID Information Element (IE)
	 * The SSID IE starts with Element ID (1 byte) and Length (1 byte)
	 * followed by the SSID itself.
	 */
	/* SSID IE format: Element ID (1) + Length (1) + SSID (variable)  */
	while (remaining_len >= 2) {
		uint8_t element_id = ie_start[0];
		uint8_t element_len = ie_start[1];

		/* Check if we have enough bytes for this IE */
		if (remaining_len < (2 + element_len)) {
			break;
		}

		/* Element ID 0 is SSID */
		/* If we find the SSID IE, check if it matches our target SSID */
		if (element_id == 0) {
			if (element_len == ssid_len &&
			    memcmp(&ie_start[2], target_ssid, ssid_len) == 0) {
				return true;
			}
			break; /* SSID should be first, so we can break here */
		}

		/* Move to next IE */
		ie_start += (2 + element_len);
		remaining_len -= (2 + element_len);
	}

	return false;
}

#ifdef CONFIG_RAW_RX_DEV_MODE_MONITOR
#define RAW_PKT_HDR 6
int raw_rx_dev_monitor_init(void)
{
	int ret;
	ret = wifi_set_mode(WIFI_MONITOR_MODE);
	if (ret) {
		LOG_ERR("Failed to set monitoring mode: %d", ret);
		return ret;
	}

	/* Set Wi-Fi regulatory domain */
	ret = wifi_set_reg_domain();
	if (ret) {
		LOG_ERR("Failed to set regulatory domain: %d", ret);
		return ret;
	}

	/* Set Wi-Fi channel for monitoring */
	ret = wifi_set_channel(CONFIG_RAW_RX_DEV_MODE_MONITOR_CHANNEL);
	if (ret) {
		LOG_ERR("Failed to set monitoring channel: %d", ret);
		return ret;
	}

	/* Initialize statistics */
	memset(&rx_stats, 0, sizeof(rx_stats));

	LOG_INF("Raw RX monitor mode initialized on channel %d",
		CONFIG_RAW_RX_DEV_MODE_MONITOR_CHANNEL);
	return 0;
}

void raw_rx_dev_monitor_task(void)
{
	int ret;
	int sockfd;
	char recv_buffer[1024];
	int recv_len;
	struct sockaddr_ll sa;
	int packet_count = 0;

	LOG_INF("Raw RX monitor task started");

	/* Create raw socket for packet capture */
	sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sockfd < 0) {
		LOG_ERR("Failed to create monitor socket: %s", strerror(errno));
		return;
	}

	sa.sll_family = AF_PACKET;
	sa.sll_ifindex = net_if_get_by_iface(net_if_get_first_wifi());

	if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		LOG_ERR("Failed to bind monitor socket: %s", strerror(errno));
		close(sockfd);
		return;
	}

	ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
			 &(struct timeval){.tv_sec = 1, .tv_usec = 0}, sizeof(struct timeval));
	if (ret < 0) {
		LOG_ERR("Failed to set socket options: %s", strerror(errno));
		close(sockfd);
		return;
	}

	LOG_INF("Monitor mode listening for raw packets...");

	while (1) {
		recv_len = recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
		if (recv_len <= 0) {
			if (errno == EAGAIN) {
				continue;
			}

			if (recv_len < 0) {
				LOG_ERR("Monitor : recv error %s", strerror(errno));
				ret = -errno;
			}
			break;
		}

		LOG_DBG("Received %d bytes", recv_len);
		if (is_raw_tx_packet_beacon((unsigned char *)(recv_buffer + RAW_PKT_HDR),
					    recv_len - RAW_PKT_HDR)) {
			led_trigger_rx();
			packet_count++;
			/* Parse received packet */
			LOG_INF("Received test packet #%u", packet_count);
		}
	}
	close(sockfd);
}
#endif /* CONFIG_RAW_RX_DEV_MODE_MONITOR */

#ifdef CONFIG_RAW_RX_DEV_MODE_PROMISCUOUS

int raw_rx_dev_promiscuous_init(void)
{
	/* Initialize statistics */
	memset(&rx_stats, 0, sizeof(rx_stats));

	LOG_INF("Raw RX promiscuous mode initialized");
	return 0;
}

void raw_rx_dev_promiscuous_task(void)
{
	int sockfd;
	char recv_buffer[CONFIG_RAW_RX_DEV_MODE_PROMISCUOUS_RECV_BUFFER_SIZE];
	int recv_len;
	struct sockaddr_ll sa;

	LOG_INF("Raw RX promiscuous task started");

	/* Create raw socket for packet capture */
	sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sockfd < 0) {
		LOG_ERR("Failed to create promiscuous socket: %s", strerror(errno));
		return;
	}

	/* Bind to Wi-Fi interface */
	struct net_if *iface = net_if_get_first_wifi();
	if (!iface) {
		LOG_ERR("Failed to get Wi-Fi interface");
		close(sockfd);
		return;
	}

	sa.sll_family = AF_PACKET;
	sa.sll_ifindex = net_if_get_by_iface(iface);

	if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		LOG_ERR("Failed to bind promiscuous socket: %s", strerror(errno));
		close(sockfd);
		return;
	}

	LOG_INF("Promiscuous mode listening for raw packets...");

	while (1) {
		recv_len = recvfrom(sockfd, recv_buffer, sizeof(recv_buffer), 0, NULL, NULL);
		if (recv_len < 0) {
			LOG_ERR("Failed to receive packet: %s", strerror(errno));
			continue;
		}

		/* Parse received packet */
		if (raw_parse_packet((unsigned char *)recv_buffer, recv_len, &rx_stats) == 0) {
			/* Successfully processed our test packet */
			LOG_DBG("Processed test packet (total: %d)", rx_stats.test_beacon_count);
		}

		/* Brief yield to prevent hogging CPU */
		k_yield();
	}

	close(sockfd);
}
#endif /* CONFIG_RAW_RX_DEV_MODE_PROMISCUOUS */
#endif /* CONFIG_WIFI_LATENCY_TEST_DEVICE_ROLE_RX */
#endif /* IS_ENABLED(CONFIG_WIFI_LATENCY_TEST_PACKET_TYPE_RAW) */