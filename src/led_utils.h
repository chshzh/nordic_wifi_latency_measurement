/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef LED_UTILS_H
#define LED_UTILS_H

#include <zephyr/kernel.h>

/**
 * @brief Initialize LED functionality
 *
 * @return 0 on success, negative error code on failure
 */
int led_init(void);

/**
 * @brief Set network status (logging only - no LED indication)
 *
 * @param connected true if network is connected, false otherwise
 */
void led_set_network_status(bool connected);

/**
 * @brief Trigger LED1 to indicate packet transmission
 */
void led_trigger_tx(void);

/**
 * @brief Trigger LED2 to indicate packet reception
 */
void led_trigger_rx(void);

#endif /* LED_UTILS_H */
