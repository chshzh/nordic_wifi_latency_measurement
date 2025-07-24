# Wi-Fi Raw TX and UDP Packet Latency Comparison

This document provides a comprehensive analysis of packet transmission paths and latency differences between raw TX packets and UDP transmission in Wi-Fi networks, including station-to-station, access point, and SoftAP scenarios.

## Table of Contents

1. [Overview](#overview)
2. [Transmission Paths](#transmission-paths)
3. [Latency Analysis](#latency-analysis)
4. [SoftAP Mode Considerations](#softap-mode-considerations)
5. [Performance Benchmarks](#performance-benchmarks)
6. [Use Case Recommendations](#use-case-recommendations)

## Overview

Wi-Fi packet transmission can follow different paths depending on the network configuration and packet type. This analysis compares two primary approaches:

- **Raw TX Packets**: Direct IEEE 802.11 frame transmission bypassing the network stack
- **UDP Packets**: Standard network layer transmission through the complete TCP/IP stack

Each method has distinct characteristics affecting latency, reliability, and implementation complexity.

## Transmission Paths

### 1. Raw TX Packet Transmission Path

**Path Structure:**
```
Application → AF_PACKET Raw Socket → Raw TX Header → IEEE 802.11 Frame → nRF70 Driver → Over-the-Air
```

**Key Characteristics:**
- **Bypasses network stack**: Goes directly to the WiFi MAC layer
- **Manual frame construction**: Application creates complete IEEE 802.11 frames
- **Direct hardware control**: Transmits exactly what you specify
- **Minimal processing overhead**: No IP/UDP header processing

**Raw TX Header Structure:**
```c
struct raw_tx_pkt_header {
    unsigned int magic_num;      // 0x12345678 (identifies raw packet)
    unsigned char data_rate;     // TX data rate/MCS
    unsigned short packet_length; // Length of 802.11 frame
    unsigned char tx_mode;       // Legacy/HT/VHT/HE mode
    unsigned char queue;         // WiFi access category
    unsigned char raw_tx_flag;   // Driver use
};
```

### 2. UDP Transmission Path

**Path Structure:**
```
Application → UDP Socket → UDP Header → IP Header → Ethernet Header → 
IEEE 802.11 Encapsulation → nRF70 Driver → Over-the-Air
```

**Key Characteristics:**
- **Full network stack**: Complete TCP/IP processing
- **Automatic encapsulation**: Stack handles all headers
- **Network layer routing**: Supports complex network topologies
- **Higher processing overhead**: Multiple protocol layers

## Latency Analysis

### Station → Station Communication Scenarios

#### 1. Direct Station-to-Station (TX Injection Mode)

**Raw TX Path:**
1. **Station A**: Creates IEEE 802.11 data frame with Station B's MAC address
2. **Over-the-Air**: Direct transmission using TX injection mode
3. **Station B**: Receives frame directly at MAC layer

**Latency**: ~0.3-3ms (air time + minimal MAC processing)

**UDP Path (Not Applicable):**
UDP requires infrastructure mode (AP) for station-to-station communication.

#### 2. Via Access Point (Infrastructure Mode)

**Raw TX Path:**
1. **Station A**: IEEE 802.11 frame → Access Point
2. **Access Point**: MAC-level forwarding based on destination address
3. **Station B**: Direct MAC layer reception

**Total latency**: ~1-5ms (air time + AP MAC processing)

**UDP Path:**
1. **Station A**: UDP packet → IP packet → Ethernet frame → IEEE 802.11 frame → AP
2. **Access Point**: 
   - Receives 802.11 frame
   - Processes IP routing
   - Forwards to Station B
3. **Station B**: Reverse encapsulation through full network stack

**Total latency**: ~10-20ms (including network stack processing at both ends)

#### 3. Via SoftAP (One Station as Access Point)

**Raw TX Path:**
- **SoftAP Station**: Acts as access point, receives raw frames directly at MAC layer
- **Client Station**: Connects to SoftAP, sends raw TX packets
- **Latency**: ~1-4ms (similar to infrastructure mode but with station-class hardware)

**UDP Path:**
- **SoftAP Station**: Runs full network stack with DHCP server, IP routing
- **Client Station**: Standard UDP socket communication
- **Latency**: ~8-15ms (reduced compared to dedicated AP due to simpler forwarding)

## SoftAP Mode Considerations

### Raw TX in SoftAP Mode

**Advantages:**
- **Low latency**: Direct MAC-layer processing in SoftAP
- **Minimal overhead**: No network stack processing on SoftAP side
- **Deterministic timing**: Predictable frame processing

**Limitations:**
- **Single-hop only**: Direct communication between SoftAP and connected station
- **Manual frame handling**: Application must process IEEE 802.11 frames
- **Limited scalability**: Suitable for point-to-point scenarios

### UDP in SoftAP Mode

**Advantages:**
- **Standard networking**: Full IP stack with DHCP, routing
- **Multiple clients**: SoftAP can handle multiple connected stations
- **Application simplicity**: Standard socket programming
- **Network services**: DNS, ARP, ICMP support

**Limitations:**
- **Higher latency**: Full network stack processing
- **Resource overhead**: DHCP server, IP routing, connection management
- **Variable timing**: Network stack introduces jitter

### SoftAP Implementation Details

**SoftAP Configuration:**
- **SSID**: Configurable access point name
- **Security**: WPA2-PSK encryption
- **Channel**: Fixed channel operation (1-11)
- **IP Range**: 192.168.4.1/24 (typical SoftAP subnet)

**Raw Socket in SoftAP:**
```c
// RX side uses AF_PACKET socket to capture raw frames
int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
```

## Performance Benchmarks

### Latency Comparison Summary

| Scenario | Raw TX Latency | UDP Latency | Improvement Factor |
|----------|----------------|-------------|-------------------|
| **Direct Station-to-Station** | 0.3-3ms | N/A (requires AP) | N/A |
| **Via Infrastructure AP** | 1-5ms | 10-20ms | 3-5x faster |
| **Via SoftAP** | 1-4ms | 8-15ms | 3-4x faster |

### Detailed Performance Characteristics

#### Raw TX Performance:
- **Best case latency**: 0.3ms (direct mode, minimal processing)
- **Typical latency**: 1-3ms (via AP or SoftAP)
- **Jitter**: <0.5ms (very consistent timing)
- **Throughput**: Limited by manual frame construction
- **CPU overhead**: Low (minimal stack processing)

#### UDP Performance:
- **Best case latency**: 8ms (SoftAP mode)
- **Typical latency**: 10-20ms (infrastructure mode)
- **Jitter**: 2-5ms (variable stack processing)
- **Throughput**: High (optimized network stack)
- **CPU overhead**: Moderate (full stack processing)

### Power Consumption Considerations

**Raw TX Mode:**
- Lower CPU usage due to bypassed network stack
- Reduced RAM usage (no network buffers)
- Potentially lower power consumption

**UDP Mode:**
- Higher CPU usage for network processing
- Additional RAM for network stack buffers
- Network services (DHCP, ARP) add periodic overhead

## Use Case Recommendations

### Choose Raw TX When:

1. **Ultra-low latency required** (< 5ms)
   - Real-time control systems
   - Time-critical sensor data
   - Industrial automation

2. **Point-to-point communication**
   - Simple device-to-device links
   - Custom protocol implementations
   - Direct sensor-to-controller scenarios

3. **Deterministic timing needed**
   - Periodic data transmission
   - Synchronization applications
   - Time-sensitive measurements

4. **SoftAP scenarios with minimal processing**
   - Single client connections
   - Custom data formats
   - Embedded systems with limited resources

### Choose UDP When:

1. **Network integration required**
   - Internet connectivity needed
   - Standard network protocols
   - Integration with existing systems

2. **Multiple device support**
   - SoftAP serving multiple clients
   - Complex network topologies
   - Scalable architectures

3. **Application portability**
   - Standard socket programming
   - Cross-platform compatibility
   - Existing network libraries

4. **Reliability features needed**
   - Automatic error handling
   - Network layer services (DHCP, DNS)
   - Connection management

### SoftAP Mode Specific Recommendations:

**Use Raw TX in SoftAP for:**
- Single high-priority client requiring low latency
- Custom industrial protocols
- Time-critical data logging
- Simple point-to-point over Wi-Fi

**Use UDP in SoftAP for:**
- Multiple client support
- Standard web services (HTTP, REST APIs)
- Configuration interfaces
- Integration with mobile apps or web browsers

## Implementation Examples

### Raw TX SoftAP Configuration:
```c
// SoftAP setup for raw packet reception
CONFIG_WIFI_RAW_TX_LATENCY_TEST_MODE_SOFTAP=y
CONFIG_WIFI_RAW_TX_LATENCY_SOFTAP_SSID="RawTX_Test_AP"
CONFIG_WIFI_RAW_TX_LATENCY_SOFTAP_CHANNEL=6

// Raw socket for packet capture
int raw_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
```

### UDP SoftAP Configuration:
```c
// SoftAP with network stack
CONFIG_WIFI_SOFTAP=y
CONFIG_NET_DHCPV4_SERVER=y
CONFIG_NET_CONFIG_MY_IPV4_ADDR="192.168.4.1"

// Standard UDP socket
int udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
```

## Conclusion

The choice between raw TX and UDP depends on specific application requirements:

- **Raw TX** provides 3-5x better latency performance and deterministic timing, ideal for time-critical applications
- **UDP** offers standard networking features, better scalability, and easier development
- **SoftAP mode** bridges the gap, providing infrastructure for both approaches with station-class hardware

For latency-critical applications requiring <5ms response times, raw TX is the preferred approach. For general networking applications requiring reliability and standard protocols, UDP remains the better choice.

Both projects provide practical implementations for measuring and comparing the latency characteristics described in this document. 