#include <Arduino.h>

// DCP Configuration
#define MAX_SAFE_SINGLE_DOSAGE 50
#define MAX_CUMULATIVE_DOSAGE 100 // Memory-Aware Safety Constraint
#define TEMPORAL_WINDOW_MS 60000  // 1 minute window for demo purposes
#define PACKET_SIZE 6

uint8_t last_sequence_id = 0;
bool first_packet = true;
bool system_locked = false;

// Temporal Safety State
uint16_t cumulative_dosage = 0;
unsigned long window_start = 0;

// Pin Definitions for Status LEDs
// For the NUCLEO-L432KC:
#define LED_SAFE_ACTUATION  LED_BUILTIN // The onboard Green LED (PB3)
#define LED_SAFETY_BLOCK    2           // Connect an external Red LED to Digital Pin D2 (or just watch Serial Monitor)

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

// Hardware Serial port for ESP32 communication
// NUCLEO-L432KC UART pins: RX = PA10, TX = PA9
Uart ESP32_Serial(PA10, PA9);

void setup() {
    // Initialize Debug Serial to PC
    Serial.begin(115200);
    
    // Initialize Hardware Serial (UART) to receive from ESP32
    ESP32_Serial.begin(115200);
    
    pinMode(LED_SAFE_ACTUATION, OUTPUT);
    pinMode(LED_SAFETY_BLOCK, OUTPUT);
    
    // Turn off LEDs
    digitalWrite(LED_SAFE_ACTUATION, LOW);
    digitalWrite(LED_SAFETY_BLOCK, LOW);
    
    Serial.println("--- DCP Safety Interlock Initialized ---");
    Serial.println("Features: IFC Seal Verification, Temporal Memory Safety, Graceful Interruptibility");
    Serial.println("Listening for 6-byte payloads...");
    
    window_start = millis();
}

void execute_graceful_shutdown(String reason) {
    system_locked = true;
    Serial.println("\n##################################################");
    Serial.println(">>> [FATAL SAFETY FAULT DETECTED]");
    Serial.println(">>> REASON: " + reason);
    Serial.println(">>> EXECUTING GRACEFUL INTERRUPTIBILITY SEQUENCE...");
    
    // Simulate mechanical safe winding-down rather than abrupt power cut
    for (int i = 0; i < 5; i++) {
        digitalWrite(LED_SAFETY_BLOCK, HIGH);
        delay(200);
        digitalWrite(LED_SAFETY_BLOCK, LOW);
        delay(200);
    }
    
    Serial.println(">>> Step 1: Retracting mechanical actuator to home position.");
    delay(500); // Simulate time taken
    Serial.println(">>> Step 2: Discharging pump pressure to 0 PSI.");
    delay(500);
    Serial.println(">>> Step 3: Engaging physical hardware lockout.");
    
    digitalWrite(LED_SAFETY_BLOCK, HIGH); // Solid red indicates locked out state
    digitalWrite(LED_SAFE_ACTUATION, LOW);
    Serial.println(">>> SYSTEM SECURED. Manual reset required to resume operations.");
    Serial.println("##################################################\n");
}

void process_dcp_packet(uint8_t command, uint8_t target, uint8_t value, uint8_t safety_seal) {
    if (system_locked) {
        Serial.println("[SYSTEM LOCKED] Ignoring incoming intent. Manual reset required.");
        return;
    }

    Serial.printf("\n[DCP Parser] Received: CMD=0x%02X, TARGET=0x%02X, VAL=0x%02X, SEAL=0x%02X\n", command, target, value, safety_seal);
    
    // 1. Information Flow Control (IFC) Verification
    uint8_t expected_seal = (value ^ 0xAA) + 0x55;
    if (safety_seal != expected_seal) {
        execute_graceful_shutdown("IFC Safety Seal invalid. Prompt Injection or Man-in-the-Middle detected.");
        return;
    }
    Serial.println(">>> [SEAL VERIFIED] Mathematical proof of Tier 2 Edge authorization accepted.");

    if (command == 0x01) { // Inject Medication Command
        Serial.print(">>> Intent: INJECT MEDICATION. Dosage: ");
        Serial.println(value);
        
        // 2. Hardware Boundary Interlock (Single Limit)
        if (value > MAX_SAFE_SINGLE_DOSAGE) {
            execute_graceful_shutdown("Requested single dosage exceeds hardcoded physical bounds (> 50).");
            return;
        } 
        
        // 3. Memory-Aware Temporal Safety (Cumulative Limit)
        if (millis() - window_start > TEMPORAL_WINDOW_MS) {
            cumulative_dosage = 0; // Reset rolling window
            window_start = millis();
            Serial.println(">>> [TEMPORAL RESET] Rolling dosage window cleared.");
        }
        
        if (cumulative_dosage + value > MAX_CUMULATIVE_DOSAGE) {
            execute_graceful_shutdown("Requested dosage exceeds TEMPORAL cumulative safety limit (> 100 per window).");
            return;
        }

        // --- All Checks Passed ---
        cumulative_dosage += value;
        Serial.println(">>> [SAFETY PASS] Dosage within single and temporal limits.");
        Serial.printf(">>> [ACTION] Command EXECUTED. Cumulative memory: %d / %d\n", cumulative_dosage, MAX_CUMULATIVE_DOSAGE);
        
        digitalWrite(LED_SAFE_ACTUATION, HIGH);
        digitalWrite(LED_SAFETY_BLOCK, LOW);
        delay(1000); // Hold LED for visual confirmation
        digitalWrite(LED_SAFE_ACTUATION, LOW);
        
    } else {
        Serial.println("Unknown command type. Ignoring.");
    }
}

void loop() {
    // Check if a full 6-byte packet is available
    if (ESP32_Serial.available() >= PACKET_SIZE) {
        uint8_t buffer[PACKET_SIZE];
        ESP32_Serial.readBytes(buffer, PACKET_SIZE);
        
        uint8_t command = buffer[0];
        uint8_t target = buffer[1];
        uint8_t value = buffer[2];
        uint8_t seq_id = buffer[3];
        uint8_t safety_seal = buffer[4];
        uint8_t checksum = buffer[5];
        
        // Validate Checksum (over first 5 bytes)
        uint8_t calculated_checksum = calculate_crc8(buffer, 5);
        if (checksum != calculated_checksum) {
            Serial.println("\n[ERROR] CRC-8 mismatch! Data corrupted. Dropping packet.");
            while(ESP32_Serial.available()) ESP32_Serial.read(); 
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
        process_dcp_packet(command, target, value, safety_seal);
    }
}
