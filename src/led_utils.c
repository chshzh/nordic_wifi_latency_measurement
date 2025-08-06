/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

#include "led_utils.h"

LOG_MODULE_REGISTER(led_utils, CONFIG_LOG_DEFAULT_LEVEL);

/* LED definitions - Only 2 LEDs available on board */
#define TX_TRIGGER_LED DK_LED1
#define RX_TRIGGER_LED DK_LED2

/* LED trigger duration in milliseconds */
#define LED_TRIGGER_DURATION_MS 50

/* Work items for LED triggers */
static struct k_work_delayable tx_led_work;
static struct k_work_delayable rx_led_work;

static void tx_led_work_handler(struct k_work *work)
{
	/* Turn off TX LED */
	dk_set_led_off(TX_TRIGGER_LED);
}

static void rx_led_work_handler(struct k_work *work)
{
	/* Turn off RX LED */
	dk_set_led_off(RX_TRIGGER_LED);
}

int led_init(void)
{
	int ret;

	LOG_INF("Initializing LEDs");

	/* Initialize DK LEDs */
	ret = dk_leds_init();
	if (ret) {
		LOG_ERR("Failed to initialize DK LEDs: %d", ret);
		return ret;
	}

	/* Initialize work items for LED triggers */
	k_work_init_delayable(&tx_led_work, tx_led_work_handler);
	k_work_init_delayable(&rx_led_work, rx_led_work_handler);

	/* Turn off all LEDs initially */
	dk_set_leds(DK_NO_LEDS_MSK);

	LOG_INF("LEDs initialized successfully");
	LOG_INF("LED1: TX trigger (flashes when transmitting packets)");
	LOG_INF("LED2: RX trigger (flashes when receiving packets)");

	return 0;
}

void led_set_network_status(bool connected)
{
	/* Network status indication removed - only 2 LEDs available */
	if (connected) {
		LOG_INF("Network connected");
	} else {
		LOG_INF("Network disconnected");
	}
}

void led_trigger_tx(void)
{
	/* Turn on TX LED */
	dk_set_led_on(TX_TRIGGER_LED);

	/* Cancel any pending work and schedule new work to turn off the LED */
	k_work_cancel_delayable(&tx_led_work);
	k_work_schedule(&tx_led_work, K_MSEC(LED_TRIGGER_DURATION_MS));
}

void led_trigger_rx(void)
{
	/* Turn on RX LED */
	dk_set_led_on(RX_TRIGGER_LED);

	/* Cancel any pending work and schedule new work to turn off the LED */
	k_work_cancel_delayable(&rx_led_work);
	k_work_schedule(&rx_led_work, K_MSEC(LED_TRIGGER_DURATION_MS));
}