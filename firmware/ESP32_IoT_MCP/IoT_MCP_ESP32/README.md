# IoT-MCP Baseline (ESP32-S3)

This is the preliminary codebase to replicate the IoT-MCP gateway baseline on the ESP32-S3.

## Goal
To process standardized JSON-RPC semantic tool calls via an MCP interface on an ESP32-S3. The framework logs response latency and tracks memory footprint (Heap/PSRAM) to benchmark against the $\sim 205$ ms latency and $\sim 74$ KB footprint targets.

## Setup Instructions for Arduino IDE

1. **Install the ESP32 Core**
   - Open Arduino IDE -> Preferences.
   - Add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` to Additional Boards Manager URLs.
   - Go to Tools -> Board -> Boards Manager, search for `esp32`, and install.

2. **Select your Board**
   - Go to Tools -> Board -> ESP32 Arduino -> **ESP32S3 Dev Module**.
   - Make sure to enable PSRAM if your specific board has it (Tools -> PSRAM -> OPI PSRAM).

3. **Install Dependencies**
   Open the Library Manager (Sketch -> Include Library -> Manage Libraries) and install:
   - **ArduinoJson** by Benoit Blanchon (Used for JSON-RPC parsing).
   - **WebSockets** by Markus Sattler (Used for the WebSocket server).

4. **Configuration**
   - Open `config.h`.
   - Replace `YOUR_WIFI_SSID` and `YOUR_WIFI_PASSWORD` with your actual Wi-Fi credentials.

5. **Upload and Test**
   - Upload the code to your ESP32-S3.
   - Open the Serial Monitor (115200 baud) to find the ESP32's IP address.
   - Use a WebSocket client (like Postman or a simple Python script) to connect to `ws://<ESP_IP>:81` and send a JSON-RPC payload:
     ```json
     {
       "jsonrpc": "2.0",
       "id": 1,
       "method": "call_tool",
       "params": {
         "name": "toggle_led",
         "arguments": {
           "state": true
         }
       }
     }
     ```
   - Check the Serial Monitor for latency metrics and memory usage benchmarking!
