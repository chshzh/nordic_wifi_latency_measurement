# Wi-Fi UDP Latency Test Application

![Nordic Semiconductor](https://img.shields.io/badge/Nordic%20Semiconductor-nRF7002-blue)
![NCS Version](https://img.shields.io/badge/NCS-v3.0.2+-green)
![Platform](https://img.shields.io/badge/Platform-nRF7002%20DK-orange)
![License](https://img.shields.io/badge/License-LicenseRef--Nordic--5--Clause-lightgrey)

> **High-precision Wi-Fi UDP latency measurement tool for Nordic nRF7002 DK with external measurement equipment integration**

## 🔍 Overview

This application is specifically designed for measuring Wi-Fi UDP round-trip latency using LED indicators synchronized with packet transmission and reception. It enables precise timing measurements using external equipment like oscilloscopes or Nordic's PPK2 (Power Profiler Kit II).

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
| **Power Supply** | USB-C | Standard development setup |

### 📍 GPIO Pin Mapping (nRF7002 DK)

| Signal | GPIO Pin | LED | Purpose |
|--------|----------|-----|---------|
| TX Trigger | P0.28 | LED1 | Transmission timing |
| RX Trigger | P0.29 | LED2 | Reception timing |

## 🚀 Quick Start

### 1. Prerequisites

Ensure you have the Nordic Connect SDK environment set up:

```bash
# Set up NCS environment (adjust path to your installation)
export ZEPHYR_BASE=/opt/nordic/ncs/v3.0.2/zephyr
export NRF_BASE=/opt/nordic/ncs/v3.0.2/nrf
source $ZEPHYR_BASE/zephyr-env.sh
```

### 2. Clone and Build

```bash
# Clone the project (adjust URL to your GitHub repository)
git clone https://github.com/yourusername/wifi_udp_latency.git
cd wifi_udp_latency

# Build for Test 1 - TX Device (External AP)
west build -p auto -b nrf7002dk/nrf5340/cpuapp -- -DEXTRA_CONF_FILE=overlay-tx.conf

# Build for Test 1 - RX Device (External AP)  
west build -p auto -b nrf7002dk/nrf5340/cpuapp -- -DEXTRA_CONF_FILE=overlay-rx.conf

# Build for Test 2 - RX Device (SoftAP)
west build -p auto -b nrf7002dk/nrf5340/cpuapp -- -DEXTRA_CONF_FILE=overlay-rx-softap.conf
```

### 3. Flash and Run

```bash
# Flash the device
west flash --erase

# Monitor serial output
screen /dev/ttyACM0 115200
```

## ⚙️ Configuration

### Wi-Fi Credentials

#### For Test 1 (External AP)
Update your Wi-Fi credentials in `overlay-tx.conf` and `overlay-rx.conf`:

```conf
CONFIG_WIFI_CREDENTIALS_STATIC_SSID="YourWiFiSSID"
CONFIG_WIFI_CREDENTIALS_STATIC_PASSWORD="YourWiFiPassword"
CONFIG_WIFI_UDP_LATENCY_TARGET_IP="192.168.1.100"  # RX device IP
```

#### For Test 2 (SoftAP)
Default SoftAP configuration in `overlay-rx-softap.conf`:

```conf
CONFIG_WIFI_UDP_LATENCY_SOFTAP_SSID="wifi-latency-test"
CONFIG_WIFI_UDP_LATENCY_SOFTAP_PSK="testpass123"
# TX device should target: CONFIG_WIFI_UDP_LATENCY_TARGET_IP="192.168.1.1"
```

### Test Parameters

| Parameter | Config Option | Default | Description |
|-----------|---------------|---------|-------------|
| UDP Port | `CONFIG_WIFI_UDP_LATENCY_UDP_PORT` | 12345 | Communication port |
| Test Duration | `CONFIG_WIFI_UDP_LATENCY_TEST_DURATION_MS` | 10000 | Test length in ms |
| Packet Interval | `CONFIG_WIFI_UDP_LATENCY_PACKET_INTERVAL_MS` | 1000 | TX interval in ms |

## 📏 Measurement Setup

### Using PPK2 (Power Profiler Kit II)

1. **Connect PPK2** to measure current spikes when LEDs flash
2. **Monitor GPIO activity** - LED current spikes indicate packet events
3. **Calculate latency** - Time difference between LED1 and LED2 spikes

### Using Oscilloscope

1. **Channel 1**: Connect to GPIO P0.28 (LED1 - TX device)
2. **Channel 2**: Connect to GPIO P0.29 (LED2 - RX device)  
3. **Trigger setup**: Rising edge on CH1
4. **Measure**: Time difference between CH1 and CH2 rising edges

```
Time between LED1 flash → LED2 flash = Wi-Fi UDP Latency
```

## 📊 Expected Results

### Normal Operation

| Device | Behavior | LED Pattern | Serial Output |
|--------|----------|-------------|---------------|
| **TX Device** | Connects to network, sends UDP packets | LED1 flashes on TX | Transmission timestamps |
| **RX Device** | Listens for packets | LED2 flashes on RX | Reception timestamps |

### Typical Latency Values

- **Local Network (same subnet)**: 1-5ms
- **Through Router**: 5-20ms  
- **SoftAP Mode**: 1-10ms

## 🐛 Troubleshooting

### Build Issues

```bash
# Clean build if configuration changes
west build -p auto -b nrf7002dk/nrf5340/cpuapp

# Check NCS environment
west --version
```

### Connection Issues

| Problem | Solution |
|---------|----------|
| No Wi-Fi connection | Verify SSID/password in overlay files |
| IP configuration | Check DHCP settings and static IP assignments |
| Device not found | Ensure both devices on same network (Test 1) |
| SoftAP not starting | Check SoftAP credentials and configuration |

### Measurement Issues

| Problem | Solution |
|---------|----------|
| No LED activity | Verify GPIO connections and LED configuration |
| Inconsistent timing | Check measurement equipment trigger levels |
| No serial output | Verify USB connection and baud rate (115200) |

## 🏗️ Project Structure

```
wifi_udp_latency/
├── src/
│   ├── main.c              # Main application logic
│   ├── wifi_utils.c/.h     # Wi-Fi management
│   ├── udp_utils.c/.h      # UDP communication
│   └── led_utils.c/.h      # LED control and timing
├── boards/                 # Board-specific configurations
├── overlay-tx.conf         # TX device configuration
├── overlay-rx.conf         # RX device (external AP)
├── overlay-rx-softap.conf  # RX device (SoftAP mode)
├── prj.conf               # Base project configuration
├── CMakeLists.txt         # Build configuration
└── sample.yaml            # Sample metadata
```

## 🤝 Contributing

We welcome contributions! Please follow these steps:

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Commit** your changes (`git commit -m 'Add amazing feature'`)
4. **Push** to the branch (`git push origin feature/amazing-feature`)
5. **Open** a Pull Request

### Development Guidelines

- Follow [Zephyr coding style](https://docs.zephyrproject.org/latest/guides/coding_guidelines/index.html)
- Add tests for new features
- Update documentation as needed
- Ensure builds pass on nRF7002 DK

## 📝 License

Copyright (c) 2025 Nordic Semiconductor ASA

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/yourusername/wifi_udp_latency/issues)
- **Discussions**: [GitHub Discussions](https://github.com/yourusername/wifi_udp_latency/discussions)
- **Nordic DevZone**: [devzone.nordicsemi.com](https://devzone.nordicsemi.com/)
- **Documentation**: [nRF Connect SDK Documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)

---

**⭐ If this project helps you, please consider giving it a star!** 