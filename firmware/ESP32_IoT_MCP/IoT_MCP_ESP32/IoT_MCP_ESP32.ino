#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include "config.h"
#include "MCP_Server.h"

// Instantiate WebSocket Server and MCP Server
WebSocketsServer webSocket = WebSocketsServer(WEBSOCKET_PORT);
MCPServer mcpServer;

// Track request timestamps for rate limiting
#define RATE_LIMIT_WINDOW_MS 1000
#define RATE_LIMIT_MAX_REQ 3
unsigned long request_times[RATE_LIMIT_MAX_REQ] = {0};
uint8_t request_index = 0;

unsigned long last_message_time = 0;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
            
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            }
            break;
            
        case WStype_TEXT:
        case WStype_BIN:
            {
                unsigned long current_time = millis();
                
                // Rate Limiting Logic
                unsigned long oldest_time = request_times[request_index];
                if (current_time - oldest_time < RATE_LIMIT_WINDOW_MS) {
                    Serial.printf("[%u] Rate limit exceeded. Disconnecting.\n", num);
                    webSocket.disconnect(num);
                    break;
                }
                
                request_times[request_index] = current_time;
                request_index = (request_index + 1) % RATE_LIMIT_MAX_REQ;
                
                last_message_time = current_time;
                
                unsigned long start_time = millis();
                
                Serial.printf("[%u] Received encrypted request, length: %u\n", num, length);
                
                // Process through MCP Server using raw bytes
                String response = mcpServer.process_rpc_call(payload, length);
                
                // Send response back
                webSocket.sendTXT(num, response);
                
                unsigned long end_time = millis();
                Serial.printf("[%u] Sent Response: %s\n", num, response.c_str());
                Serial.printf("--> Latency: %lu ms\n", (end_time - start_time));
                
                // Print memory after transaction to monitor peak usage
                mcpServer.print_memory_usage();
            }
            break;
            
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200); // PC Debugging
    // Initialize UART2 for STM32 communication (Baud: 115200, RX: 16, TX: 17)
    Serial2.begin(115200, SERIAL_8N1, 16, 17);
    
    delay(1000);
    
    // Memory footprint check at startup
    Serial.println("\n--- Starting IoT-MCP Baseline ---");
    mcpServer.print_memory_usage();

        // Connect to Wi-Fi
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(WIFI_SSID);
    
    WiFi.mode(WIFI_STA); // Explicitly set to Station mode
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Force it to wait until the router assigns an IP
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("");
    Serial.print("SUCCESS! IP address: ");
    Serial.println(WiFi.localIP());
    
    // Start WebSocket Server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.printf("WebSocket Server started on port %d\n", WEBSOCKET_PORT);
    
    last_message_time = millis();
}

void loop() {
    // Process WebSocket events
    webSocket.loop();
    
    // Check for EdgeShard Timeout (Layer 3 Fallback trigger)
    if (millis() - last_message_time > 5000) {
        mcpServer.run_tinyml_fallback();
        last_message_time = millis(); // Reset to prevent spamming
    }
}
