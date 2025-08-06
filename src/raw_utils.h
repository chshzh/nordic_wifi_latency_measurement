/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef RAW_UTILS_H
#define RAW_UTILS_H

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>

/* External declarations for shared TX control variables */
extern struct k_sem tx_start_sem;
extern volatile bool tx_task_running;
extern volatile bool tx_task_should_stop;

/* Raw packet header structure */
struct raw_tx_pkt_header {
	unsigned int magic_num;
	unsigned char data_rate;
	unsigned short packet_length;
	unsigned char tx_mode;
	unsigned char queue;
	unsigned char raw_tx_flag;
};

/* Beacon frame structure for raw transmission */
struct beacon_frame {
	uint16_t frame_control;
	uint16_t duration;
	uint8_t da[6];
	uint8_t sa[6];
	uint8_t bssid[6];
	uint16_t seq_ctrl;
	uint8_t payload[256];
} __packed;

/* Frame control structure for parsing received frames */
typedef struct {
	uint16_t protocolVersion: 2;
	uint16_t type: 2;
	uint16_t subtype: 4;
	uint16_t toDS: 1;
	uint16_t fromDS: 1;
	uint16_t moreFragments: 1;
	uint16_t retry: 1;
	uint16_t powerManagement: 1;
	uint16_t moreData: 1;
	uint16_t protectedFrame: 1;
	uint16_t order: 1;
} frame_control_t;

/* Packet statistics structure */
typedef struct {
	int beacon_count;
	int test_beacon_count; /* Beacons from our test application */
	int data_count;
	int total_count;
	int64_t first_packet_timestamp;
	int64_t last_packet_timestamp;
} raw_packet_stats_t;

/**
 * @brief Initialize raw packet transmission
 *
 * @return 0 on success, negative error code on failure
 */
int raw_tx_init(void);

/**
 * @brief Initialize raw packet reception in monitor mode
 *
 * @return 0 on success, negative error code on failure
 */
int raw_tx_socket_init(void);

/**
 * @brief Send a raw packet with timing measurement
 *
 * @param packet_num Packet sequence number for identification
 * @return 0 on success, negative error code on failure
 */
int raw_tx_send_packet(uint32_t packet_num);

/**
 * @brief Cleanup raw packet transmission
 */
void raw_tx_cleanup(void);

/**
 * @brief Initialize raw packet reception (monitor mode)
 *
 * @return 0 on success, negative error code on failure
 */
int raw_rx_dev_monitor_init(void);

/**
 * @brief Initialize raw packet reception (promiscuous mode)
 *
 * @return 0 on success, negative error code on failure
 */
int raw_rx_dev_promiscuous_init(void);

/**
 * @brief Main raw packet RX task function (monitor mode)
 *
 * This function runs the main reception loop for raw packets in monitor mode
 */
void raw_rx_dev_monitor_task(void);

/**
 * @brief Main raw packet RX task function (promiscuous mode)
 *
 * This function runs the main reception loop for raw packets in promiscuous mode
 */
void raw_rx_dev_promiscuous_task(void);

/**
 * @brief Parse received raw packet and extract timing information
 *
 * @param packet Pointer to the received packet data
 * @param packet_len Length of the received packet
 * @param stats Pointer to statistics structure to update
 * @return 0 if packet is recognized, negative if not our test packet
 */
int raw_parse_packet(unsigned char *packet, int packet_len, raw_packet_stats_t *stats);

/**
 * @brief Check if a packet is from our test application
 *
 * @param packet Pointer to the packet data
 * @param packet_len Length of the packet
 * @return true if it's our test packet, false otherwise
 */
bool raw_is_test_packet(unsigned char *packet, int packet_len);

#endif /* RAW_UTILS_H */
