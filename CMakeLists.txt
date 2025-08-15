#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wifi_udp_latency)

# Common source files
target_sources(app PRIVATE
    src/main.c
    src/wifi_utils.c
    src/udp_utils.c
    src/led_utils.c
    src/raw_utils.c
    src/net_event_mgmt_utils.c
) 