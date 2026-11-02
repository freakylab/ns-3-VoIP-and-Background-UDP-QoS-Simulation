# ns3-voip-qos-toggle
*Simulation NS3: How effectively can Strict Priority QoS eliminate VoIP packet loss compared to a standard Best-Effort FIFO queue*
-------------------------------------------------------------------------------------------------------------------------------

This project contains an ns-3 simulation (`voip-background-toggle-v8.cc`) that evaluates network performance when a latency-sensitive VoIP stream competes with an aggressive UDP background stream over a shared bottleneck link.

The purpose of this simulation is to demonstrate the difference between a standard FIFO (First-In-First-Out) queue and a Strict Priority queue (QoS - Quality of Service) for VoIP traffic.

## Network Topology
The simulation creates a four-node network (Node 0 to Node 3):
* **Node 0 (VoIP Sender)** ──(50 Mbps, 1ms)──> **Node 2 (Router)**
* **Node 1 (Background Sender)** ──(50 Mbps, 1ms)──> **Node 2 (Router)**
* **Node 2 (Router)** ──(10 Mbps, 25ms)──> **Node 3 (Receiver)**

The link between **Node 2 and Node 3** acts as a bottleneck with a limited capacity of 10 Mbit/s. Both the VoIP and background traffic must traverse this link, which creates network congestion.

## Traffic Flows
* **VoIP Stream:** Sent from Node 0 to Node 3 via UDP (Port 5000). It has a constant bit rate of 64 kbps and is marked with the ToS (Type of Service) value `0xb8` to indicate high priority.
* **Background Stream:** Sent from Node 1 to Node 3 via UDP (Port 9000). It sends data at a high rate (default 12 Mbit/s) with the ToS value `0x00`.

## Queue Management and QoS
The total queue capacity on the bottleneck link is 100 packets. The program can be run in two different modes:
* **QoS OFF (FIFO - `--qos=0`):** All packets (both VoIP and background) are placed in the same queue (`FifoQueueDisc`). If the queue fills up, packets are dropped regardless of the traffic type.
* **QoS ON (Strict Priority - `--qos=1`):** The router uses a priority queue (`PrioQueueDisc`). The queue is divided into two classes: 
    * **High Priority (VoIP):** Allocated space for 2 packets. Traffic marked with ToS `0xb8` is placed here and is always dequeued and sent before traffic in the lower class.
    * **Low Priority (Background):** Allocated space for the remaining 98 packets.

## Getting Started
To run this code, you need to have the ns-3 network simulator installed.

### 1. Place the File
Copy the file `voip-background-toggle-v8.cc` and place it in the `scratch/` directory of your ns-3 installation.

### 2. Run Commands
You run the program via the terminal from the root of your ns-3 directory. Arguments must be passed inside quotes in the run command.

**Example 1: Run simulation WITHOUT QoS (FIFO)**
In this scenario, VoIP packets compete equally with 12 Mbit/s of background traffic over a 10 Mbit/s link.
```bash
./ns3 run "scratch/voip-background-toggle-v8 --qos=0 --background=12.0"
```

**Example 2: Run simulation WITH QoS (Strict Priority)**
Here, VoIP packets are given absolute priority, which should result in minimal delay and zero packet loss for the voice stream.
```bash
./ns3 run "scratch/voip-background-toggle-v8 --qos=1 --background=12.0"
```

## Available Parameters (Arguments)
You can customize the simulation using the following command-line arguments:

| Parameter | Description | Default Value |
| :--- | :--- | :--- |
| `--qos` | `0` = FIFO (no QoS); `1` = Strict VoIP priority | `0` (False) |
| `--background` | Bitrate for the background traffic in Mbit/s | `12.0` |
| `--csv` | Filename where the results will be saved | `voip-qos-background-results.csv` |

*For example, to run with QoS enabled, a massive background traffic of 20 Mbit/s, and save to a specific file:*
```bash
./ns3 run "scratch/voip-background-toggle-v8 --qos=1 --background=20.0 --csv=my_results.csv"
```

## Results and Output
When the simulation (which lasts for exactly 32 seconds) finishes, two things happen:
* **Terminal Output:** A detailed summary is printed directly to the console. It shows the number of packets sent, packets received, packet loss (in percent), and the mean one-way delay for both VoIP and background traffic.
* **CSV File:** The program creates (or appends a new row to) the file `voip-qos-background-results.csv`. This file is perfect for generating graphs to visualize how delay and packet loss change with and without QoS across different levels of background traffic.

**CSV File Columns:**
`qos, background_mbps, voip_mean_delay_ms, voip_loss_percent, background_loss_percent, background_mean_delay_ms, background_rx_goodput_mbps`