#include "MCP_Server.h"
#include "mbedtls/gcm.h"

static uint8_t sequence_id = 0;

uint8_t calculate_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}

MCPServer::MCPServer() {
    // Initialization code if needed
}

String MCPServer::process_rpc_call(uint8_t* payload, size_t length) {
    // The payload format is: IV (12 bytes) + Tag (16 bytes) + Ciphertext
    if (length < 28) {
        return "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32000,\"message\":\"Payload too short for GCM\"},\"id\":null}";
    }

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    unsigned char key[] = "1234567890123456";
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 128);

    unsigned char* iv = payload;
    unsigned char* tag = payload + 12;
    unsigned char* ciphertext = payload + 28;
    size_t ciphertext_len = length - 28;

    unsigned char* output = new unsigned char[ciphertext_len + 1];
    memset(output, 0, ciphertext_len + 1);

    int ret = mbedtls_gcm_auth_decrypt(&gcm, ciphertext_len, 
                                       iv, 12, 
                                       NULL, 0, 
                                       tag, 16, 
                                       ciphertext, output);

    mbedtls_gcm_free(&gcm);

    if (ret != 0) {
        delete[] output;
        Serial.println("[SECURITY ALERT] AES-GCM Authentication Failed!");
        return "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32000,\"message\":\"GCM Auth Failed\"},\"id\":null}";
    }

    String decrypted_request = String((char*)output);
    delete[] output;

    // Target ~74KB peak memory baseline. 
    // We allocate a JSONDocument large enough for standard RPC payloads.
    JsonDocument doc; 
    
    DeserializationError error = deserializeJson(doc, decrypted_request);
    
    JsonDocument responseDoc;
    
    if (error) {
        responseDoc["jsonrpc"] = "2.0";
        responseDoc["error"]["code"] = -32700;
        responseDoc["error"]["message"] = "Parse error";
        responseDoc["id"] = nullptr;
    } else {
        String method = doc["method"] | "";
        int id = doc["id"] | -1;
        JsonObject params = doc["params"];

        responseDoc["jsonrpc"] = "2.0";
        responseDoc["id"] = id;

        if (method == "call_tool") {
            String tool_name = params["name"] | "";
            JsonObject tool_params = params["arguments"];
            
            String result = execute_tool(tool_name, tool_params);
            responseDoc["result"]["content"] = result;
        } else {
            responseDoc["error"]["code"] = -32601;
            responseDoc["error"]["message"] = "Method not found";
        }
    }

    String responseStr;
    serializeJson(responseDoc, responseStr);
    return responseStr;
}

String MCPServer::execute_tool(String tool_name, JsonObject parameters) {
    if (tool_name == "inject_medication") {
        return tool_inject_medication(parameters);
    } else if (tool_name == "toggle_led") {
        return tool_toggle_led(parameters);
    } else if (tool_name == "read_sensor") {
        return tool_read_sensor(parameters);
    }
    
    return "Error: Unknown tool - " + tool_name;
}

String MCPServer::tool_inject_medication(JsonObject parameters) {
    uint8_t patient_id = parameters["patient_id"] | 0;
    uint8_t dosage = parameters["dosage"] | 0;
    uint8_t safety_seal = parameters["safety_seal"] | 0;
    
    // Semantic Degradation: Convert intent to DCP Byte-Array
    // Byte 0: Command (0x01 = Inject)
    // Byte 1: Target (Patient ID)
    // Byte 2: Value (Dosage)
    // Byte 3: Sequence ID
    // Byte 4: Safety Seal (IFC)
    // Byte 5: CRC8
    
    uint8_t command = 0x01;
    sequence_id++;
    
    uint8_t dcp_payload[6] = {command, patient_id, dosage, sequence_id, safety_seal, 0};
    dcp_payload[5] = calculate_crc8(dcp_payload, 5);
    
    // Transmit to Extreme Edge (STM32) via Serial2
    Serial2.write(dcp_payload, 6);
    
    // Add prominent logging since jumper cables are missing
    Serial.println("\n==================================================");
    Serial.println(">>> SEMANTIC DEGRADATION SUCCESSFUL");
    Serial.println(">>> JSON Intent translated to 6-byte DCP Array (with IFC Seal).");
    Serial.printf(">>> TX -> STM32: [ 0x%02X | 0x%02X | 0x%02X | 0x%02X | 0x%02X | 0x%02X ]\n", 
                  dcp_payload[0], dcp_payload[1], dcp_payload[2], dcp_payload[3], dcp_payload[4], dcp_payload[5]);
    Serial.println("==================================================\n");
                  
    return "Command delegated to Extreme Edge for Formal Verification.";
}

String MCPServer::tool_toggle_led(JsonObject parameters) {
    bool state = parameters["state"] | false;
    // Dummy logic for hardware actuation
    // digitalWrite(LED_PIN, state ? HIGH : LOW);
    return state ? "LED turned ON" : "LED turned OFF";
}

String MCPServer::tool_read_sensor(JsonObject parameters) {
    String sensor_type = parameters["type"] | "temperature";
    // Dummy sensor read
    if (sensor_type == "temperature") {
        return "36.5C"; // e.g. normal body temp
    } else if (sensor_type == "heart_rate") {
        return "75bpm";
    }
    return "Unknown sensor type";
}

void MCPServer::print_memory_usage() {
    Serial.println("--- Memory Benchmark ---");
    Serial.print("Free Heap: ");
    Serial.print(ESP.getFreeHeap() / 1024.0);
    Serial.println(" KB");

    Serial.print("Free PSRAM: ");
    Serial.print(ESP.getFreePsram() / 1024.0);
    Serial.println(" KB");
    Serial.println("------------------------");
}

void MCPServer::run_tinyml_fallback() {
    Serial.println("\n[ESP32 TinyML] WARNING: EdgeShard Connection Lost!");
    Serial.println("[ESP32 TinyML] Activating Layer 3 Autonomous Fallback...");
    
    // Simulate local sensor read
    uint8_t hr = 160; 
    uint8_t spo2 = 88;
    
    uint8_t dosage = 0;
    // TinyML Decision Tree
    if (hr > 140) {
        dosage = 20; // Safe dosage in autonomous mode
    } else if (hr > 120) {
        dosage = 10;
    }
    
    uint8_t patient_id = 10;
    uint8_t command = 0x01; // Inject
    sequence_id++;
    
    // Calculate deterministic seal internally for tinyML fallback
    uint8_t safety_seal = (dosage ^ 0xAA) + 0x55;
    
    uint8_t dcp_payload[6] = {command, patient_id, dosage, sequence_id, safety_seal, 0};
    dcp_payload[5] = calculate_crc8(dcp_payload, 5);
    
    Serial2.write(dcp_payload, 6);
    
    Serial.println("==================================================");
    Serial.println(">>> AUTONOMOUS TINYML ACTUATION TRIGGERED");
    Serial.printf(">>> TX -> STM32: [ 0x%02X | 0x%02X | 0x%02X | 0x%02X | 0x%02X | 0x%02X ]\n", 
                  dcp_payload[0], dcp_payload[1], dcp_payload[2], dcp_payload[3], dcp_payload[4], dcp_payload[5]);
    Serial.println("==================================================\n");
}
