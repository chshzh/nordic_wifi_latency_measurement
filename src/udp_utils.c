/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>

#include "udp_utils.h"

LOG_MODULE_REGISTER(udp_utils, CONFIG_LOG_DEFAULT_LEVEL);

int udp_client_init(int *socket, struct sockaddr_in *server_addr, const char *target_ip,
		    uint16_t port)
{
	int sock;
	int ret;

	/* Create UDP socket */
	sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_ERR("Failed to create UDP socket: %d", errno);
		return -errno;
	}

	/* Configure server address */
	server_addr->sin_family = AF_INET;
	server_addr->sin_port = htons(port);

	ret = zsock_inet_pton(AF_INET, target_ip, &server_addr->sin_addr);
	if (ret <= 0) {
		LOG_ERR("Invalid target IP address: %s", target_ip);
		zsock_close(sock);
		return -EINVAL;
	}

	*socket = sock;
	LOG_INF("UDP client initialized, target: %s:%d", target_ip, port);
	return 0;
}

int udp_server_init(int *socket, uint16_t port)
{
	int sock;
	struct sockaddr_in server_addr;
	int ret;

	/* Create UDP socket */
	sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_ERR("Failed to create UDP socket: %d", errno);
		return -errno;
	}

	/* Configure server address */
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	/* Bind socket */
	ret = zsock_bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		LOG_ERR("Failed to bind UDP socket: %d", errno);
		zsock_close(sock);
		return -errno;
	}

	*socket = sock;
	LOG_INF("UDP server initialized on port %d", port);
	return 0;
}

int udp_send(int socket, struct sockaddr_in *server_addr, const char *data, size_t data_len)
{
	int ret;

	ret = zsock_sendto(socket, data, data_len, 0, (struct sockaddr *)server_addr,
			   sizeof(*server_addr));
	if (ret < 0) {
		LOG_ERR("Failed to send UDP data: %d", errno);
		return -errno;
	}

	return ret;
}

int udp_receive(int socket, char *buffer, size_t buffer_size)
{
	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	int ret;

	ret = zsock_recvfrom(socket, buffer, buffer_size - 1, 0, (struct sockaddr *)&client_addr,
			     &client_addr_len);
	if (ret < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			/* Timeout or would block - not an error */
			return 0;
		}
		LOG_ERR("Failed to receive UDP data: %d", errno);
		return -errno;
	}

	return ret;
}

void udp_client_cleanup(int socket)
{
	if (socket >= 0) {
		zsock_close(socket);
		LOG_INF("UDP client socket closed");
	}
}

void udp_server_cleanup(int socket)
{
	if (socket >= 0) {
		zsock_close(socket);
		LOG_INF("UDP server socket closed");
	}
}