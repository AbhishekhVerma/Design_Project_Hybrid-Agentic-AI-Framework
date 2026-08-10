# Hybrid Agentic AI Framework for Intelligent IoT Decision-Making

This repository contains the source code for a four-tier hybrid edge-cloud architecture designed to enforce hardware-level safety isolation for Agentic AI in mission-critical Cyber-Physical Systems (CPS). The system is primarily evaluated using an autonomous Mass Casualty Incident (MCI) triage simulation.

## Architecture Overview

The system degrades high-level semantic AI intents into deterministic byte-arrays across four tiers:

1. **Tier 1 (Cloud):** A Multi-Agent Classroom (Triage, Medical, and Review Board agents) powered by the Gemini API.
2. **Tier 2 (Edge PC):** Asynchronous Python middleware that orchestrates the agents, generates a rigid JSON-RPC payload, and encrypts it using AES-128-GCM.
3. **Tier 3 (ESP32-S3 Gateway):** An intermediate gateway that decrypts the payload, validates the authentication tag, and parses the JSON within a strict heap memory footprint. It translates the payload into a 5-byte Device Control Protocol (DCP) array.
4. **Tier 4 (STM32 NUCLEO-L432KC):** The extreme edge hardware safety interlock. It verifies the DCP payload via CRC-8 checksums and monotonic sequence IDs, physically blocking any command that violates hardcoded boundary constraints.

## Repository Structure

* `middleware/` - Contains the Python-based Edge PC orchestrator (`llm_client.py`).
* `firmware/` - Contains the C++ code for the ESP32-S3 IoT-MCP Gateway and the STM32 DCP actuator.

## Setup Instructions

### 1. Python Middleware (Tier 2)
1. Navigate to the `middleware/EdgeShard_PC/` directory.
2. Install the required Python packages (e.g., `google-genai`, `pycryptodome`, `websockets`).
3. Create a `.env` file in the `middleware/EdgeShard_PC/` directory and add your Gemini API key:
   ```
   GEMINI_API_KEY=your_api_key_here
   ```

### 2. ESP32 Gateway (Tier 3)
1. Open the `firmware/ESP32_IoT_MCP/IoT_MCP_ESP32/` project in the Arduino IDE.
2. Create a `secrets.h` file in this directory to hold your Wi-Fi credentials (this file is ignored by Git for security):
   ```cpp
   #ifndef SECRETS_H
   #define SECRETS_H
   #define SECRET_WIFI_SSID "your_wifi_ssid"
   #define SECRET_WIFI_PASSWORD "your_wifi_password"
   #endif
   ```
3. Compile and upload to the ESP32-S3.

### 3. Execution
Run the Python middleware to initiate the Multi-Agent triage simulation:
```bash
python middleware/EdgeShard_PC/llm_client.py
```
To test the local offline fallback mechanism (bypassing the cloud API), run:
```bash
python middleware/EdgeShard_PC/llm_client.py --offline
```

## Security Note
This repository includes a strict `.gitignore` policy. Ensure you do not accidentally commit your `.env` or `secrets.h` files to version control.
