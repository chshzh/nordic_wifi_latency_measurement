/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NET_MGMT_H
#define NET_MGMT_H

#include <zephyr/kernel.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>

/**
 * @brief Initialize network event handlers
 * 
 * Sets up all network management event callbacks for different layers
 * 
 * @return 0 on success, negative error code on failure
 */
int init_network_events(void);
 
#endif /* NET_MGMT_H */
