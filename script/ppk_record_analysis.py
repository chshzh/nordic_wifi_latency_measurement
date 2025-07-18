#!/usr/bin/env python3
"""
PPK2 Recording Analysis Script for UDP Latency Measurement

This script analyzes PPK2 digital channel recordings to calculate UDP transmission latency.
It detects trigger events on D0 (TX) and D1 (RX) channels and calculates the time difference.

Usage:
    python ppk_record_analysis.py -i udp_softap.csv

Author: Nordic Semiconductor
License: LicenseRef-Nordic-5-Clause
"""

import argparse
import csv
import sys
from typing import List, Tuple, Optional

class TriggerEvent:
    """Represents a trigger event with timestamp and channel"""
    def __init__(self, timestamp: float, channel: str):
        self.timestamp = timestamp
        self.channel = channel
    
    def __repr__(self):
        return f"TriggerEvent(t={self.timestamp:.3f}ms, ch={self.channel})"

class LatencyMeasurement:
    """Represents a latency measurement between TX and RX triggers"""
    def __init__(self, packet_num: int, tx_time: float, rx_time: float):
        self.packet_num = packet_num
        self.tx_time = tx_time
        self.rx_time = rx_time
        self.latency = rx_time - tx_time
    
    def __repr__(self):
        return f"Packet{self.packet_num}: {self.latency:.3f}ms"

def detect_rising_edges(timestamps: List[float], channel_data: List[int]) -> List[float]:
    """
    Detect rising edges (0 -> 1 transitions) in digital channel data
    
    Args:
        timestamps: List of timestamp values in milliseconds
        channel_data: List of digital values (0 or 1)
    
    Returns:
        List of timestamps where rising edges occur
    """
    rising_edges = []
    prev_value = 0
    
    for i, (timestamp, value) in enumerate(zip(timestamps, channel_data)):
        if prev_value == 0 and value == 1:
            rising_edges.append(timestamp)
        prev_value = value
    
    return rising_edges

def parse_csv_file(filename: str) -> Tuple[List[float], List[float]]:
    """
    Parse PPK2 CSV file and extract TX (D0) and RX (D1) trigger timestamps
    
    Args:
        filename: Path to the CSV file
    
    Returns:
        Tuple of (tx_triggers, rx_triggers) timestamp lists
    """
    timestamps = []
    d0_data = []
    d1_data = []
    
    try:
        with open(filename, 'r', newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            
            # Verify required columns exist
            fieldnames = reader.fieldnames
            if fieldnames is None:
                raise ValueError("CSV file has no column headers")
            if 'Timestamp(ms)' not in fieldnames:
                raise ValueError("CSV file must contain 'Timestamp(ms)' column")
            if 'D0' not in fieldnames or 'D1' not in fieldnames:
                raise ValueError("CSV file must contain 'D0' and 'D1' columns")
            
            print(f"Parsing CSV file: {filename}")
            print(f"Columns found: {fieldnames}")
            
            row_count = 0
            for row in reader:
                try:
                    timestamp = float(row['Timestamp(ms)'])
                    d0_value = int(row['D0'])
                    d1_value = int(row['D1'])
                    
                    timestamps.append(timestamp)
                    d0_data.append(d0_value)
                    d1_data.append(d1_value)
                    
                    row_count += 1
                    if row_count % 100000 == 0:
                        print(f"Processed {row_count} rows...")
                        
                except (ValueError, KeyError) as e:
                    print(f"Warning: Skipping invalid row {row_count}: {e}")
                    continue
            
            print(f"Successfully parsed {row_count} data points")
            
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found")
        sys.exit(1)
    except Exception as e:
        print(f"Error reading CSV file: {e}")
        sys.exit(1)
    
    # Detect rising edges
    print("Detecting trigger events...")
    tx_triggers = detect_rising_edges(timestamps, d0_data)
    rx_triggers = detect_rising_edges(timestamps, d1_data)
    
    print(f"Found {len(tx_triggers)} TX triggers (D0)")
    print(f"Found {len(rx_triggers)} RX triggers (D1)")
    
    return tx_triggers, rx_triggers

def match_triggers_and_calculate_latency(tx_triggers: List[float], rx_triggers: List[float], 
                                       max_latency_ms: float = 100.0) -> List[LatencyMeasurement]:
    """
    Match TX and RX triggers and calculate latency measurements
    
    Args:
        tx_triggers: List of TX trigger timestamps
        rx_triggers: List of RX trigger timestamps  
        max_latency_ms: Maximum expected latency to filter out invalid matches
    
    Returns:
        List of LatencyMeasurement objects
    """
    measurements = []
    used_rx_indices = set()
    
    print("Matching TX and RX triggers...")
    
    for packet_num, tx_time in enumerate(tx_triggers):
        best_match = None
        best_latency = float('inf')
        best_idx = -1
        
        # Find the closest RX trigger after this TX trigger
        for i, rx_time in enumerate(rx_triggers):
            if i in used_rx_indices:
                continue
                
            # RX must be after TX
            if rx_time <= tx_time:
                continue
            
            latency = rx_time - tx_time
            
            # Filter out unreasonably high latencies
            if latency > max_latency_ms:
                continue
            
            # Find the closest valid RX trigger
            if latency < best_latency:
                best_latency = latency
                best_match = rx_time
                best_idx = i
        
        if best_match is not None:
            measurements.append(LatencyMeasurement(packet_num, tx_time, best_match))
            used_rx_indices.add(best_idx)
        else:
            print(f"Warning: No matching RX trigger found for TX trigger {packet_num} at {tx_time:.3f}ms")
    
    print(f"Successfully matched {len(measurements)} packet pairs")
    return measurements

def generate_markdown_table(measurements: List[LatencyMeasurement]) -> str:
    """
    Generate markdown table from latency measurements
    
    Args:
        measurements: List of LatencyMeasurement objects
    
    Returns:
        Markdown formatted table string
    """
    if not measurements:
        return "No measurements available."
    
    # Calculate statistics
    latencies = [m.latency for m in measurements]
    avg_latency = sum(latencies) / len(latencies)
    min_latency = min(latencies)
    max_latency = max(latencies)
    
    # Generate table header
    table = "## UDP Latency Analysis Results\n\n"
    table += f"**Summary Statistics:**\n"
    table += f"- Total Packets: {len(measurements)}\n"
    table += f"- Average Latency: {avg_latency:.3f} ms\n"
    table += f"- Minimum Latency: {min_latency:.3f} ms\n"
    table += f"- Maximum Latency: {max_latency:.3f} ms\n\n"
    
    table += "| Packet Number | TX Trigger Time (ms) | RX Trigger Time (ms) | Latency (ms) |\n"
    table += "|---------------|---------------------|---------------------|-------------|\n"
    
    # Add data rows
    for measurement in measurements:
        table += f"| {measurement.packet_num} | {measurement.tx_time:.2f} | {measurement.rx_time:.2f} | {measurement.latency:.2f} |\n"
    
    # Add summary rows
    table += "|---------------|---------------------|---------------------|-------------|\n"
    table += f"| **Average** | - | - | **{avg_latency:.2f}** |\n"
    table += f"| **Minimum** | - | - | **{min_latency:.2f}** |\n"
    table += f"| **Maximum** | - | - | **{max_latency:.2f}** |\n"
    
    return table

def main():
    """Main function to handle command line arguments and orchestrate analysis"""
    parser = argparse.ArgumentParser(
        description="Analyze PPK2 CSV recordings for UDP latency measurement",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python ppk_record_analysis.py -i udp_softap.csv
    python ppk_record_analysis.py -i my_recording.csv -m 50.0

The script expects CSV format with columns:
    Timestamp(ms), D0, D1, D2, D3, D4, D5, D6, D7
    
Where:
    - D0: TX trigger channel (0 -> 1 transition indicates packet transmission)
    - D1: RX trigger channel (0 -> 1 transition indicates packet reception)
        """
    )
    
    parser.add_argument('-i', '--input', 
                       required=True,
                       help='Input CSV file path (PPK2 recording)')
    
    parser.add_argument('-m', '--max-latency',
                       type=float,
                       default=300.0,
                       help='Maximum expected latency in ms (default: 300.0)')
    
    parser.add_argument('-o', '--output',
                       help='Output file for markdown table (default: stdout)')
    
    args = parser.parse_args()
    
    # Parse CSV file and detect triggers
    tx_triggers, rx_triggers = parse_csv_file(args.input)
    
    if not tx_triggers:
        print("Error: No TX triggers found in the data")
        sys.exit(1)
    
    if not rx_triggers:
        print("Error: No RX triggers found in the data")
        sys.exit(1)
    
    # Match triggers and calculate latency
    measurements = match_triggers_and_calculate_latency(tx_triggers, rx_triggers, args.max_latency)
    
    if not measurements:
        print("Error: No valid latency measurements could be calculated")
        sys.exit(1)
    
    # Generate markdown table
    markdown_table = generate_markdown_table(measurements)
    
    # Output results
    if args.output:
        try:
            with open(args.output, 'w') as f:
                f.write(markdown_table)
            print(f"Results saved to: {args.output}")
        except Exception as e:
            print(f"Error writing output file: {e}")
            sys.exit(1)
    else:
        print("\n" + "="*80)
        print("MARKDOWN TABLE OUTPUT")
        print("="*80)
        print(markdown_table)
        print("="*80)

if __name__ == "__main__":
    main() 