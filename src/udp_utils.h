/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef UDP_UTILS_H
#define UDP_UTILS_H

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>

/**
 * @brief Initialize UDP client
 *
 * @param socket Pointer to store the socket descriptor
 * @param server_addr Pointer to store the server address structure
 * @param target_ip Target IP address as string
 * @param port Target port number
 * @return 0 on success, negative error code on failure
 */
int udp_client_init(int *socket, struct sockaddr_in *server_addr, const char *target_ip,
		    uint16_t port);

/**
 * @brief Initialize UDP server
 *
 * @param socket Pointer to store the socket descriptor
 * @param port Port number to listen on
 * @return 0 on success, negative error code on failure
 */
int udp_server_init(int *socket, uint16_t port);

/**
 * @brief Send UDP packet
 *
 * @param socket Socket descriptor
 * @param server_addr Server address structure
 * @param data Data to send
 * @param data_len Length of data to send
 * @return Number of bytes sent on success, negative error code on failure
 */
int udp_send(int socket, struct sockaddr_in *server_addr, const char *data, size_t data_len);

/**
 * @brief Receive UDP packet
 *
 * @param socket Socket descriptor
 * @param buffer Buffer to store received data
 * @param buffer_size Size of the buffer
 * @return Number of bytes received on success, negative error code on failure
 */
int udp_receive(int socket, char *buffer, size_t buffer_size);

/**
 * @brief Cleanup UDP client
 *
 * @param socket Socket descriptor to close
 */
void udp_client_cleanup(int socket);

/**
 * @brief Cleanup UDP server
 *
 * @param socket Socket descriptor to close
 */
void udp_server_cleanup(int socket);

#endif /* UDP_UTILS_H */
