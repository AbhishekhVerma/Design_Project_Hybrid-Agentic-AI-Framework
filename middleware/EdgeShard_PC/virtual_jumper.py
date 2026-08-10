import serial
import serial.tools.list_ports
import re
import argparse
import sys
import time

def main():
    parser = argparse.ArgumentParser(description="Virtual Jumper Cable (ESP32 -> STM32 Bridge)")
    parser.add_argument("--esp", type=str, required=True, help="COM port for ESP32 (e.g. COM3)")
    parser.add_argument("--stm", type=str, required=True, help="COM port for STM32 (e.g. COM4)")
    args = parser.parse_args()

    print(f"[*] Opening ESP32 port {args.esp} at 115200 baud...")
    print(f"[*] Opening STM32 port {args.stm} at 115200 baud...")
    
    try:
        esp_serial = serial.Serial(args.esp, 115200, timeout=0.1)
        stm_serial = serial.Serial(args.stm, 115200, timeout=0.1)
    except Exception as e:
        print(f"[!] Error opening serial ports: {e}")
        print("\nAvailable ports:")
        for p in serial.tools.list_ports.comports():
            print(f" - {p.device}: {p.description}")
        sys.exit(1)

    print("[*] Bridge active! Waiting for DCP arrays from the ESP32...")
    print("-" * 60)

    # Regex to match: >>> TX -> STM32: [ 0x01 | 0x0A | 0x14 | 0x01 | 0x8C ]
    dcp_pattern = re.compile(r">>> TX -> STM32: \[ 0x([0-9A-Fa-f]{2}) \| 0x([0-9A-Fa-f]{2}) \| 0x([0-9A-Fa-f]{2}) \| 0x([0-9A-Fa-f]{2}) \| 0x([0-9A-Fa-f]{2}) \]")

    try:
        while True:
            # 1. Read from ESP32
            if esp_serial.in_waiting:
                try:
                    esp_line = esp_serial.readline().decode('utf-8', errors='ignore').strip()
                    if esp_line:
                        print(f"[ESP32] {esp_line}")
                        
                        match = dcp_pattern.search(esp_line)
                        if match:
                            # Extract the 5 hex bytes
                            bytes_list = [int(match.group(i), 16) for i in range(1, 6)]
                            dcp_payload = bytes(bytes_list)
                            
                            print(f"\n[BRIDGE] Detected DCP Array. Forwarding {len(dcp_payload)} bytes to STM32...")
                            
                            # 2. Forward to STM32
                            stm_serial.write(dcp_payload)
                            print(f"[BRIDGE] Transmitted: {list(dcp_payload)}\n")
                            
                except Exception as e:
                    pass

            # 3. Read from STM32 (Listen for safety validations/actuations)
            if stm_serial.in_waiting:
                try:
                    stm_line = stm_serial.readline().decode('utf-8', errors='ignore').strip()
                    if stm_line:
                        print(f"[STM32] {stm_line}")
                except:
                    pass
                    
            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\n[*] Bridge closed.")
        esp_serial.close()
        stm_serial.close()

if __name__ == "__main__":
    main()
