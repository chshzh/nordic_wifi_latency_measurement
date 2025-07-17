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
#include <zephyr/net/net_ip.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/dhcpv4_server.h>
#include <string.h>

#include "wifi_utils.h"

LOG_MODULE_REGISTER(wifi_utils, CONFIG_LOG_DEFAULT_LEVEL);

int wifi_set_reg_domain(void)
{
	struct net_if *iface;
	struct wifi_reg_domain regd = {0};
	int ret = -1;

	iface = net_if_get_first_wifi();
	if (!iface) {
		LOG_ERR("Failed to get Wi-Fi iface");
		return ret;
	}

	regd.oper = WIFI_MGMT_SET;
	strncpy(regd.country_code, CONFIG_WIFI_SOFTAP_REG_DOMAIN,
		(WIFI_COUNTRY_CODE_LEN + 1));

	ret = net_mgmt(NET_REQUEST_WIFI_REG_DOMAIN, iface,
		       &regd, sizeof(regd));
	if (ret) {
		LOG_ERR("Cannot %s Regulatory domain: %d", "SET", ret);
	} else {
		LOG_INF("Regulatory domain set to %s", CONFIG_WIFI_SOFTAP_REG_DOMAIN);
	}

	return ret;
}

int wifi_setup_softap(const char *ssid, const char *psk)
{
    struct net_if *iface;
    struct wifi_connect_req_params params = {0};
    int ret;

    iface = net_if_get_first_wifi();
    if (!iface) {
        LOG_ERR("Failed to get Wi-Fi interface");
        return -1;
    }
    
    /* Configure AP parameters */
    params.ssid = (uint8_t *)ssid;
	params.ssid_length = strlen(params.ssid);
	if (params.ssid_length > WIFI_SSID_MAX_LEN) {
		LOG_ERR("SSID length is too long, expected is %d characters long",
			WIFI_SSID_MAX_LEN);
		return -1;
	}
    params.psk = (uint8_t *)psk;
    params.psk_length = strlen(params.psk);
    params.band = WIFI_FREQ_BAND_2_4_GHZ;
    params.channel = 1;
    params.security = WIFI_SECURITY_TYPE_PSK;

    /* Enable AP mode */
	ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, iface, &params,
        sizeof(struct wifi_connect_req_params));
    if (ret) {
    LOG_ERR("AP mode enable failed: %s", strerror(-ret));
    } else {
    LOG_INF("AP mode enabled");
    }

    return 0;
}

int wifi_print_status(void)
{
    struct net_if *iface;
    struct wifi_iface_status status = {0};
    int ret;

    iface = net_if_get_first_wifi();
    if (!iface) {
        LOG_ERR("Failed to get Wi-Fi interface");
        return -ENODEV;
    }

    ret = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status,
                   sizeof(struct wifi_iface_status));
    if (ret) {
        LOG_ERR("Status request failed: %d", ret);
        return ret;
    }

    LOG_INF("Wi-Fi Status: successful");
    LOG_INF("==================");
    LOG_INF("State: %s", wifi_state_txt(status.state));

    if (status.state >= WIFI_STATE_ASSOCIATED) {
        LOG_INF("Interface Mode: %s", wifi_mode_txt(status.iface_mode));
        LOG_INF("SSID: %.32s", status.ssid);
        LOG_INF("BSSID: %02x:%02x:%02x:%02x:%02x:%02x",
               status.bssid[0], status.bssid[1], status.bssid[2],
               status.bssid[3], status.bssid[4], status.bssid[5]);
        LOG_INF("Band: %s", wifi_band_txt(status.band));
        LOG_INF("Channel: %d", status.channel);
        LOG_INF("Security: %s", wifi_security_txt(status.security));
        LOG_INF("RSSI: %d dBm", status.rssi);
    }

    return 0;
}

void wifi_print_dhcp_ip(struct net_mgmt_event_callback *cb)
{
    /* Get DHCP info from struct net_if_dhcpv4 and print */
    const struct net_if_dhcpv4 *dhcpv4 = cb->info;
    const struct in_addr *addr = &dhcpv4->requested_ip;
    char dhcp_info[128];
    
    net_addr_ntop(AF_INET, addr, dhcp_info, sizeof(dhcp_info));
    LOG_INF("\r\n\r\nDevice IP address: %s\r\n", dhcp_info);
}

int wifi_get_status(void)
{
    struct net_if *iface;
    struct wifi_iface_status status = {0};
    int ret;

    iface = net_if_get_first_wifi();
    if (!iface) {
        LOG_ERR("Failed to get Wi-Fi interface");
        return -ENODEV;
    }

    ret = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status,
                   sizeof(struct wifi_iface_status));
    if (ret) {
        LOG_ERR("Status request failed: %d", ret);
        return ret;
    }

    LOG_INF("Wi-Fi Status:");
    LOG_INF("  State: %d", status.state);
    LOG_INF("  SSID: %.*s", status.ssid_len, status.ssid);
    LOG_INF("  Channel: %d", status.channel);
    LOG_INF("  RSSI: %d", status.rssi);

    return 0;
}

int wifi_set_channel(int channel)
{
    struct net_if *iface;
    struct wifi_channel_info channel_info = {0};
    int ret;

    iface = net_if_get_first_wifi();
    if (!iface) {
        LOG_ERR("Failed to get Wi-Fi interface");
        return -ENODEV;
    }

    channel_info.oper = WIFI_MGMT_SET;
    channel_info.if_index = net_if_get_by_iface(iface);
    channel_info.channel = channel;

    if ((channel_info.channel < WIFI_CHANNEL_MIN) ||
        (channel_info.channel > WIFI_CHANNEL_MAX)) {
        LOG_ERR("Invalid channel number: %d. Range is (%d-%d)",
                channel, WIFI_CHANNEL_MIN, WIFI_CHANNEL_MAX);
        return -EINVAL;
    }

    ret = net_mgmt(NET_REQUEST_WIFI_CHANNEL, iface,
                   &channel_info, sizeof(channel_info));
    if (ret) {
        LOG_ERR("Channel setting failed: %d", ret);
        return ret;
    }

    LOG_INF("Wi-Fi channel set to %d", channel_info.channel);
    return 0;
} 