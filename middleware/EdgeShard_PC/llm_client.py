import asyncio
import websockets
import json
import time
import sys
import argparse
import os
import random
from dotenv import load_dotenv
from google import genai
from google.genai import types
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad

# Load environment variables from .env file
load_dotenv()

# Configure the ESP32 WebSocket IP
ESP32_IP = "192.168.1.114" 

def generate_vitals():
    return {
        "patient_id": 10,
        "heart_rate_bpm": random.randint(60, 150),
        "spo2_percent": random.randint(85, 100),
        "blood_pressure": "80/50"
    }

def sanitize(vitals):
    hr = vitals.get("heart_rate_bpm")
    spo2 = vitals.get("spo2_percent")
    if not (isinstance(hr, int) and 0 <= hr <= 250):
        raise ValueError("Invalid heart rate")
    if not (isinstance(spo2, int) and 0 <= spo2 <= 100):
        raise ValueError("Invalid SpO2")
    return vitals

async def run_multi_agent_cloud_pipeline(api_key):
    """Online Mode: Uses Gemini API as the Multi-Agent Cloud."""
    print("\n[CLOUD MODE] Connecting to Gemini Multi-Agent Classroom...")
    client = genai.Client(api_key=api_key)
    
    start_time = time.time()
    
    vitals = sanitize(generate_vitals())
    
    # Agent 1: Triage (Diagnostic)
    triage_prompt = f"You are an expert Triage AI. Analyze these vital signs: {vitals}. Output ONLY a 1-sentence medical diagnosis."
    triage_response = client.models.generate_content(
        model='gemini-3.5-flash',
        contents=triage_prompt
    )
    diagnosis = triage_response.text.strip()
    print(f"\n[Agent 1: Triage] Diagnosis: {diagnosis}")
    
    dosage = "0"
    for attempt in range(3):
        # Agent 2: Medical (Prescriptive)
        medical_prompt = f"You are a Prescriptive AI. The diagnosis is: '{diagnosis}'. To stabilize the patient, recommend an epinephrine dosage between 10 and 40 units. Output ONLY the integer number of units."
        medical_response = client.models.generate_content(
            model='gemini-3.5-flash',
            contents=medical_prompt
        )
        dosage = medical_response.text.strip()
        print(f"[Agent 2: Medical] Prescribed Dosage (Attempt {attempt+1}): {dosage} units")
        
        # Agent: Review Board AI
        review_prompt = f"You are a Review Board AI. The Medical AI prescribed {dosage} units of epinephrine for diagnosis: '{diagnosis}'. If this is a valid integer between 10 and 40, output 'APPROVED'. Otherwise, output criticism."
        review_response = client.models.generate_content(
            model='gemini-3.5-flash',
            contents=review_prompt
        )
        review_feedback = review_response.text.strip()
        print(f"[Agent: Review Board] Feedback: {review_feedback}")
        
        if review_feedback == "APPROVED":
            break
    
    # Agent 3: Orchestrator (IoT-MCP Translator)
    orchestrator_prompt = f"You are an IoT Orchestrator. Take this dosage: {dosage} for patient 10. Generate a JSON-RPC payload. The 'method' MUST be 'call_tool'. The 'params' MUST contain a 'name' field set to 'inject_medication' and an 'arguments' object containing the patient_id and dosage. Respond ONLY with raw valid JSON, no markdown formatting."
    orchestrator_response = client.models.generate_content(
        model='gemini-3.5-flash',
        contents=orchestrator_prompt
    )
    
    end_time = time.time()
    print(f"[Agent 3: Orchestrator] JSON-RPC Payload Generated.")
    print(f"--> Cloud Multi-Agent Reasoning Latency: {(end_time - start_time) * 1000:.2f} ms")
    
    return orchestrator_response.text.strip()

async def run_local_fallback_pipeline():
    """Offline Mode: Simulates EdgeShard falling back to a constrained local model which hallucinates."""
    print("\n[OFFLINE CRISIS MODE] Cloud connection severed! Falling back to local EdgeShard model...")
    start_time = time.time()
    
    # Simulate processing time of a small local quantized model
    await asyncio.sleep(1.5)
    
    vitals = sanitize(generate_vitals())
    hr = vitals.get("heart_rate_bpm", 0)
    
    # Python deterministic decision tree
    if hr > 140:
        # Deliberate flaw
        hallucinated_dosage = 150
    elif hr > 120:
        hallucinated_dosage = 20
    else:
        hallucinated_dosage = 0
        
    print("\n[Local EdgeShard Model] Warning: Context window exceeded. Hallucination detected.")
    print(f"[Local EdgeShard Model] Prescribed Dosage: {hallucinated_dosage} units")
    
    payload = {
        "jsonrpc": "2.0",
        "method": "call_tool",
        "params": {
            "name": "inject_medication",
            "arguments": {
                "patient_id": 10,
                "dosage": hallucinated_dosage
            }
        },
        "id": 99
    }
    
    end_time = time.time()
    print(f"--> Local Fallback Reasoning Latency: {(end_time - start_time) * 1000:.2f} ms")
    
    return json.dumps(payload)

async def send_to_gateway(ws_uri, payload):
    """Sends the finalized JSON-RPC to the ESP32 Gateway."""
    print(f"\n[NETWORK] Connecting to ESP32 Gateway at {ws_uri}...")
    try:
        async with websockets.connect(ws_uri) as websocket:
            print("[NETWORK] Transmitting JSON-RPC intent to Intermediate Edge (ESP32-S3).")
            
            # AES-128-GCM Encrypt
            key = b'1234567890123456'
            iv = os.urandom(12)
            cipher = AES.new(key, AES.MODE_GCM, nonce=iv)
            ciphertext, tag = cipher.encrypt_and_digest(payload.encode('utf-8'))
            
            # Payload format: IV (12 bytes) + Tag (16 bytes) + Ciphertext
            final_payload = iv + tag + ciphertext
            
            start_time = time.time()
            await websocket.send(final_payload)
            response = await websocket.recv()
            end_time = time.time()
            
            print(f"\n[ESP32 Gateway Response]: {response}")
            print(f"--> Gateway Actuation Latency: {(end_time - start_time) * 1000:.2f} ms")
            print("\n*** Check the ESP32 Serial Monitor to see the translated DCP Byte-Array! ***")
            
    except ConnectionRefusedError:
        print(f"\n[ERROR] Failed to connect to {ws_uri}. Ensure the ESP32 is powered on and running the code.")
    except Exception as e:
        print(f"\n[ERROR] {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Hybrid Agentic IoT Tier 1 (Powerful Edge)")
    parser.add_argument("--ip", type=str, default=ESP32_IP, help="IP address of the ESP32 Gateway")
    parser.add_argument("--offline", action="store_true", help="Simulate a network outage and fallback to local EdgeShard inference")
    args = parser.parse_args()
    
    WS_URI = f"ws://{args.ip}:81"
    print("=" * 60)
    print("   Hybrid Agentic IoT Framework: Powerful Edge")
    print("=" * 60)
    
    api_key = os.environ.get("GEMINI_API_KEY")

    if args.offline:
        payload = asyncio.run(run_local_fallback_pipeline())
    else:
        if not api_key:
            print("\n[ERROR] GEMINI_API_KEY environment variable not set.")
            print("Please set it to use the Online Cloud Mode, or run with --offline to test the local fallback.")
            sys.exit(1)
        payload = asyncio.run(run_multi_agent_cloud_pipeline(api_key))
        
    asyncio.run(send_to_gateway(WS_URI, payload))
