# Wi-Fi UDP Packet Latency Test Application

![Nordic Semiconductor](https://img.shields.io/badge/Nordic%20Semiconductor-nRF7002-blue)
![NCS Version](https://img.shields.io/badge/NCS-v3.0.2+-green)
![Platform](https://img.shields.io/badge/Platform-nRF7002%20DK-orange)
![License](https://img.shields.io/badge/License-LicenseRef--Nordic--5--Clause-lightgrey)

> **High-precision Wi-Fi UDP latency measurement tool for Nordic nRF7002 DK with external measurement equipment integration**

## 🔍 Overview

This application is specifically designed for measuring Wi-Fi UDP latency using LED indicators synchronized with packet transmission and reception. It enables precise timing measurements using external equipment like oscilloscopes or Nordic's PPK2 (Power Profiler Kit II).

### 🎯 Key Features

- **📊 Precise Latency Measurement**: Hardware-triggered LED indicators for accurate timing
- **🔄 Dual Test Scenarios**: Station-to-Station via external AP, or direct SoftAP mode
- **⚡ GPIO Timing Triggers**: Synchronized LED flashes for external measurement equipment
- **🔧 Configurable Parameters**: Adjustable test duration, intervals, and network settings
- **📈 Real-time Monitoring**: Serial logging with detailed timing information
- **🛠️ Modular Design**: Clean separation of Wi-Fi, UDP, and LED control modules

## 🧪 Test Scenarios

### Test 1: External Access Point Mode
```
[TX Device] ──WiFi──> [External AP] ──WiFi──> [RX Device]
    ↓                                             ↓
  LED1 Flash                                  LED2 Flash
```

Both devices connect to an external Wi-Fi access point for communication.

### Test 2: SoftAP Mode  
```
[TX Device] ──WiFi──> [RX Device (SoftAP)]
    ↓                        ↓
  LED1 Flash            LED2 Flash
```

RX device creates a SoftAP, TX device connects directly to it.

## 🔧 Hardware Requirements

| Component | Specification | Notes |
|-----------|---------------|-------|
| **Main Board** | nRF7002 DK | Primary target platform |
| **NCS Version** | v3.0.2 or later | Nordic Connect SDK |
| **Measurement Equipment** | PPK2 or Oscilloscope | For precise timing measurement |

### 📍 GPIO Pin Mapping (nRF7002 DK)

| Signal | GPIO Pin | LED | Purpose |
|--------|----------|-----|---------|
| TX Trigger | P1.06 | LED1 | Transmission timing |
| RX Trigger | P1.07 | LED2 | Reception timing |

## 🏗️ Project Structure

```
wifi_udp_packet_latency/
├── src/
│   ├── main.c              # Main application logic
│   ├── wifi_utils.c/.h     # Wi-Fi management
│   ├── udp_utils.c/.h      # UDP communication
│   └── led_utils.c/.h      # LED control and timing
├── script/
│   └── ppk_record_analysis.py  # PPK2 CSV analysis tool
├── boards/                 # Board-specific configurations
├── overlay-tx.conf         # TX device configuration
├── overlay-rx.conf         # RX device (external AP)
├── overlay-rx-softap.conf  # RX device (SoftAP mode)
├── prj.conf               # Base project configuration
├── CMakeLists.txt         # Build configuration
└── sample.yaml            # Sample metadata
```

## 🚀 Quick Start

### 1. Prerequisites

Ensure you have the [Nordic Connect SDK environment](https://docs.nordicsemi.com/bundle/ncs-3.0.2/page/nrf/installation/install_ncs.html) set up.


### 2. Clone and Build

```bash
# Clone the project (adjust URL to your GitHub repository)
git clone https://github.com/chshzh/wifi_udp_packet_latency.git
cd wifi_udp_packet_latency

# Build for TX Device (Test 1 and Test 2) - Standard
west build -p -b nrf7002dk/nrf5340/cpuapp -- -DEXTRA_CONF_FILE=overlay-tx.conf

# Build for Test 1 - RX Device (External AP) - Standard
west build -p -b nrf7002dk/nrf5340/cpuapp -- -DEXTRA_CONF_FILE=overlay-rx.conf

# Build for Test 2 - RX Device (SoftAP) - Standard
west build -p -b nrf7002dk/nrf5340/cpuapp -- -DEXTRA_CONF_FILE=overlay-rx-softap.conf
```

### 3. Flash and Run

```bash
# Flash the device
west flash 

## ⚙️ Configuration

### Wi-Fi Credentials

#### For Test 1 (External AP)
Update your Wi-Fi credentials in `overlay-tx.conf` and `overlay-rx.conf`:

```conf
CONFIG_WIFI_CREDENTIALS_STATIC_SSID="YourWiFiSSID"
CONFIG_WIFI_CREDENTIALS_STATIC_PASSWORD="YourWiFiPassword"
CONFIG_WIFI_UDP_PACKET_LATENCY_TARGET_IP="192.168.1.100"  # RX device IP
```

#### For Test 2 (SoftAP)
Default SoftAP configuration in `overlay-rx-softap.conf` for RX device:

```conf
CONFIG_WIFI_UDP_PACKET_LATENCY_SOFTAP_SSID="wifi-latency-test"
CONFIG_WIFI_UDP_PACKET_LATENCY_SOFTAP_PSK="testpass123"
```

Update your Wi-Fi credentials in `overlay-tx.conf` for TX device:

```conf
CONFIG_WIFI_CREDENTIALS_STATIC_SSID="wifi-latency-test"
CONFIG_WIFI_CREDENTIALS_STATIC_PASSWORD="testpass123"
CONFIG_WIFI_UDP_PACKET_LATENCY_TARGET_IP="192.168.1.1"  # RX device IP
```

### Test Parameters

| Parameter | Config Option | Default | Description |
|-----------|---------------|---------|-------------|
| UDP Port | `CONFIG_WIFI_UDP_PACKET_LATENCY_UDP_PORT` | 12345 | Communication port |
| Test Duration | `CONFIG_WIFI_UDP_PACKET_LATENCY_TEST_DURATION_MS` | 10000 | Test length in ms |
| Packet Interval | `CONFIG_WIFI_UDP_PACKET_LATENCY_PACKET_INTERVAL_MS` | 1000 | TX interval in ms |

## 🎮 Hardware Interface

| Component | Location | Function |
|-----------|----------|----------|
| **Button 1** | SW1 on nRF7002 DK | TX device Start/restart transmission |
| **Serial Console** | USB CDC-ACM | Status and log output |


## 📏 Measurement Setup

### Using PPK2 (Power Profiler Kit II has /10us Resolution)

1. **Connect PPK2** to measure current spikes when LEDs flash
2. **Monitor GPIO activity** - LED current spikes indicate packet events
3. **Calculate latency** - Time difference between LED1 and LED2 spikes
4. **Resolution**- Maximum 100000 samples per second results 10us resolution

#### Pin Connection

![ppk2 pin connection](photo/ppk2_connection.png)

#### Power Profiler 

![ppk2 pin connection](photo/powerprofiler.png)

#### PPK2 Data Analysis

After recording with PPK2, use the provided analysis script to automatically calculate UDP latencies:

```bash
# Navigate to script directory
cd script/

# Run analysis on PPK2 CSV export
python ppk_record_analysis.py -i ppk_recording.csv

# Save results to file (optional)
python ppk_record_analysis.py -i ppk_recording.csv -o results.md

# Customize maximum expected latency threshold (default: 300ms)
python ppk_record_analysis.py -i ppk_recording.csv -m 50.0
```

**Script Features:**
- **Automatic trigger detection**: Identifies D0 (TX) and D1 (RX) rising edge events
- **Intelligent matching**: Pairs TX and RX triggers with closest temporal proximity
- **Statistical analysis**: Calculates average, minimum, and maximum latencies
- **Markdown output**: Ready-to-copy tables for documentation
- **Error handling**: Validates data format and filters invalid measurements

**Expected CSV format** (PPK2 export):
```
Timestamp(ms),D0,D1,D2,D3,D4,D5,D6,D7
0,0,0,1,1,1,1,1,1
0.01,0,0,1,1,1,1,1,1
0.02,1,0,1,1,1,1,1,1  ← D0 rising edge (TX trigger)
0.03,1,1,1,1,1,1,1,1  ← D1 rising edge (RX trigger)
```

**Example Output:**
```
## UDP Latency Analysis Results

**Summary Statistics:**
- Total Packets: 10
- Average Latency: 13.045 ms
- Minimum Latency: 7.530 ms
- Maximum Latency: 14.010 ms

| Packet Number | TX Trigger Time (ms) | RX Trigger Time (ms) | Latency (ms) |
|---------------|---------------------|---------------------|-------------|
| 0 | 7519.09 | 7531.35 | 12.26 |
| 1 | 8520.57 | 8534.18 | 13.61 |
| 2 | 9521.56 | 9529.09 | 7.53 |
...
|---------------|---------------------|---------------------|-------------|
| **Average** | - | - | **13.05** |
| **Minimum** | - | - | **7.53** |
| **Maximum** | - | - | **14.01** |
```

### Using Oscilloscope

1. **Channel 1**: Connect to GPIO P1.06 (LED1 - TX device)
2. **Channel 2**: Connect to GPIO P1.07 (LED2 - RX device)  
3. **Trigger setup**: Rising edge on CH1
4. **Measure**: Time difference between CH1 and CH2 rising edges
4. **Resolution**- Depends on Oscilloscope sampling rate
```
Time between LED1 flash → LED2 flash = Wi-Fi UDP Latency
```

## 📊 Results

### Normal Operation

| Device | Behavior | LED Pattern | Serial Output |
|--------|----------|-------------|---------------|
| **TX Device** | Connects to network, sends UDP packets | LED1 flashes on TX | Transmission timestamps |
| **RX Device** | Listens for packets | LED2 flashes on RX | Reception timestamps |

### Test 1: External Access Point Mode

| Packet Number | TX Trigger Time (ms) | RX Trigger Time (ms) | Latency (ms) |
|---------------|---------------------|---------------------|-------------|
| 0 | 826.70 | 843.81 | 17.11 |
| 1 | 1843.63 | 1857.05 | 13.42 |
| 2 | 2860.23 | 2864.12 | 3.89 |
| 3 | 3876.83 | 3891.86 | 15.03 |
| 4 | 4893.43 | 4906.61 | 13.18 |
| 5 | 5910.03 | 5932.83 | 22.80 |
| 6 | 6926.69 | 6934.11 | 7.42 |
| 7 | 7943.35 | 7962.77 | 19.42 |
| 8 | 8959.92 | 8989.10 | 29.18 |
| 9 | 9976.55 | 9989.36 | 12.81 |
|---------------|---------------------|---------------------|-------------|
| **Average** | - | - | **15.43** |
| **Minimum** | - | - | **3.89** |
| **Maximum** | - | - | **29.18** |

### Test 2: SoftAP Mode 


| Packet Number | TX Trigger Time (ms) | RX Trigger Time (ms) | Latency (ms) |
|---------------|---------------------|---------------------|-------------|
| 0 | 1449.94 | 1452.93 | 2.99 |
| 1 | 2466.78 | 2469.18 | 2.40 |
| 2 | 3483.35 | 3487.05 | 3.70 |
| 3 | 4499.92 | 4505.45 | 5.53 |
| 4 | 5516.55 | 5519.04 | 2.49 |
| 5 | 6533.12 | 6538.37 | 5.25 |
| 6 | 7549.66 | 7563.68 | 14.02 |
| 7 | 8565.87 | 8571.47 | 5.60 |
| 8 | 9582.43 | 9585.32 | 2.89 |
| 9 | 10598.97 | 10601.91 | 2.94 |
|---------------|---------------------|---------------------|-------------|
| **Average** | - | - | **4.78** |
| **Minimum** | - | - | **2.40** |
| **Maximum** | - | - | **14.02** |

## 📖 Documentation

### [Wi-Fi Raw and UDP Packet Latency Comparison](wifi_raw_and_udp_packets_latency_comparison.md)

Comprehensive analysis document covering:
- **Transmission path comparisons** between Raw TX and UDP approaches
- **SoftAP mode considerations** for both Raw TX and UDP scenarios  
- **Performance benchmarks** with detailed latency measurements
- **Use case recommendations** for choosing the optimal approach
- **Implementation examples** for different network configurations

This document provides the theoretical foundation and practical guidance for understanding when to use Raw TX vs UDP transmission methods. It complements this project by explaining the latency differences and trade-offs between the two approaches.

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/chshzh/wifi_udp_packet_latency/issues)
- **Discussions**: [GitHub Discussions](https://github.com/chshzh/wifi_udp_packet_latency/discussions)
- **Nordic DevZone**: [devzone.nordicsemi.com](https://devzone.nordicsemi.com/)
- **Documentation**: [nRF Connect SDK Documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)

## 📝 License

Copyright (c) 2025 Nordic Semiconductor ASA

[SPDX-License-Identifier: LicenseRef-Nordic-5-Clause](LICENSE)
---

**⭐ If this project helps you, please consider giving it a star!** 