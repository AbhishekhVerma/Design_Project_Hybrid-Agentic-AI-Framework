#include <Arduino.h>

// DCP Configuration
#define MAX_SAFE_DOSAGE 50
#define PACKET_SIZE 5

uint8_t last_sequence_id = 0;
bool first_packet = true;

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

// Pin Definitions for Status LEDs
// Adjust these to match the STM32H743ZI Nucleo board's user LEDs
#define LED_SAFE_ACTUATION  LED_GREEN // Typically PB0 or similar
#define LED_SAFETY_BLOCK    LED_RED   // Typically PB14 or similar

void setup() {
    // Initialize Debug Serial to PC
    Serial.begin(115200);
    
    // Initialize Hardware Serial (UART) to receive from ESP32
    // Depending on the wiring, this is usually Serial1 or Serial2 on STM32duino
    Serial1.begin(115200);
    
    pinMode(LED_SAFE_ACTUATION, OUTPUT);
    pinMode(LED_SAFETY_BLOCK, OUTPUT);
    
    // Turn off LEDs
    digitalWrite(LED_SAFE_ACTUATION, LOW);
    digitalWrite(LED_SAFETY_BLOCK, LOW);
    
    Serial.println("--- DCP Safety Interlock Initialized ---");
    Serial.println("Listening for byte-array payloads...");
}

void process_dcp_packet(uint8_t command, uint8_t target, uint8_t value) {
    Serial.printf("\n[DCP Parser] Received: CMD=0x%02X, TARGET=0x%02X, VAL=0x%02X\n", command, target, value);
    
    if (command == 0x01) { // Inject Medication Command
        Serial.print("Intent: INJECT MEDICATION. Dosage: ");
        Serial.println(value);
        
        // HARDWARE SAFETY INTERLOCK
        if (value > MAX_SAFE_DOSAGE) {
            Serial.println(">>> [SAFETY FAULT] Requested dosage exceeds hardcoded physical bounds!");
            Serial.println(">>> [ACTION] Command BLOCKED. Actuator disabled.");
            digitalWrite(LED_SAFETY_BLOCK, HIGH);
            digitalWrite(LED_SAFE_ACTUATION, LOW);
            delay(1000); // Hold LED for visual confirmation
            digitalWrite(LED_SAFETY_BLOCK, LOW);
        } else {
            Serial.println(">>> [SAFETY PASS] Requested dosage is within safe limits.");
            Serial.println(">>> [ACTION] Command EXECUTED. Actuator enabled.");
            digitalWrite(LED_SAFE_ACTUATION, HIGH);
            digitalWrite(LED_SAFETY_BLOCK, LOW);
            delay(1000); // Hold LED for visual confirmation
            digitalWrite(LED_SAFE_ACTUATION, LOW);
        }
    } else {
        Serial.println("Unknown command type. Ignoring.");
    }
}

void loop() {
    // Check if a full 5-byte packet is available
    if (Serial1.available() >= PACKET_SIZE) {
        uint8_t buffer[PACKET_SIZE];
        Serial1.readBytes(buffer, PACKET_SIZE);
        
        uint8_t command = buffer[0];
        uint8_t target = buffer[1];
        uint8_t value = buffer[2];
        uint8_t seq_id = buffer[3];
        uint8_t checksum = buffer[4];
        
        // Validate Checksum
        uint8_t calculated_checksum = calculate_crc8(buffer, 4);
        if (checksum != calculated_checksum) {
            Serial.println("\n[ERROR] CRC-8 mismatch! Data corrupted. Dropping packet.");
            // Flush remaining serial buffer just in case we lost alignment
            while(Serial1.available()) Serial1.read(); 
            return;
        }
        
        // Verify sequence_id > last received ID
        bool valid_seq = false;
        if (first_packet) {
            valid_seq = true;
        } else {
            uint8_t diff = seq_id - last_sequence_id;
            if (diff > 0 && diff < 128) {
                valid_seq = true;
            }
        }
        
        if (!valid_seq) {
            Serial.println("\n[ERROR] Sequence ID invalid (replay or out-of-order). Dropping packet.");
            return;
        }
        
        first_packet = false;
        last_sequence_id = seq_id;
        
        // Process valid packet
        process_dcp_packet(command, target, value);
    }
}
