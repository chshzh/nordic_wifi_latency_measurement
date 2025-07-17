/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef WIFI_UTILS_H
#define WIFI_UTILS_H

#include <zephyr/kernel.h>
#include <zephyr/net/net_mgmt.h>

/**
 * @brief Set up SoftAP mode
 * 
 * @param ssid SSID for the SoftAP
 * @param psk Password for the SoftAP
 * @return 0 on success, negative error code on failure
 */
int wifi_setup_softap(const char *ssid, const char *psk);

/**
 * @brief Get current Wi-Fi status
 * 
 * @return 0 on success, negative error code on failure
 */
int wifi_get_status(void);

/**
 * @brief Print detailed Wi-Fi status information
 * 
 * @return 0 on success, negative error code on failure
 */
int wifi_print_status(void);

/**
 * @brief Print DHCP IP address when bound
 * 
 * @param cb Network management event callback containing DHCP info
 */
void wifi_print_dhcp_ip(struct net_mgmt_event_callback *cb);

/**
 * @brief Set Wi-Fi channel (for non-connected mode)
 * 
 * @param channel Channel number to set
 * @return 0 on success, negative error code on failure
 */
int wifi_set_channel(int channel);

/**
 * @brief Set Wi-Fi regulatory domain
 * 
 * @return 0 on success, negative error code on failure
 */
int wifi_set_reg_domain(void);

#endif /* WIFI_UTILS_H */ 