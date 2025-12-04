#!/usr/bin/env python3
"""
plant_detect_sms.py - Final TFLite version
- Waits for PlantID from Arduino on /dev/ttyUSB0
- Captures image with Raspberry Pi Camera (libcamera)
- Runs TFLite model (128x128 preprocessing)
- Sends SMS via SIM800 to:
    1) Farmer
    2) Control Unit / Supervisor
- Logs detection to CSV
- Sends 'DONE' to Arduino to move to next plant
- Images saved as: plant_<PlantID>-day1.jpg
"""

import os
import time
import serial
import subprocess
import numpy as np
from datetime import datetime
import csv
from tensorflow.keras.preprocessing import image as kimage
import tflite_runtime.interpreter as tflite

# === CONFIG ===
ARDUINO_PORT = "/dev/ttyUSB0"
SIM800_PORT  = "/dev/serial0"
SIM800_BAUD  = 115200
ARDUINO_BAUD = 9600
MODEL_PATH   = "/home/pi/diseases_dragon_fruit_trained_3+1_float32.tflite"
TARGET_NUMBER= "+911234567890"   # Farmer
CU_NUMBER     = "+919876543210"   # Control Unit
IMAGE_DIR    = "/home/pi/plant_images"
IMAGE_SIZE   = (128, 128)
CLASS_NAMES  = ['anthracnose', 'cactusvirusx', 'healthy', 'stemcanker']
CAPTURE_COMMAND = "libcamera-jpeg"
LOG_FILE = os.path.join(IMAGE_DIR, "plant_log.csv")
# =================

os.makedirs(IMAGE_DIR, exist_ok=True)

# Create CSV if it doesn't exist
if not os.path.exists(LOG_FILE):
    with open(LOG_FILE, mode='w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(["Timestamp","PlantID","Label","Confidence","ImageFile"])

# Load TFLite model
print("Loading TFLite model...")
interpreter = tflite.Interpreter(model_path=MODEL_PATH)
interpreter.allocate_tensors()
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()
print("TFLite model loaded.")

# === Functions ===

def capture_image_for_plant(plant_id, out_path):
    """Capture image using libcamera-jpeg."""
    cmd = [CAPTURE_COMMAND, "-o", out_path, "--nopreview"]
    try:
        subprocess.run(cmd, check=True, timeout=10)
        print(f"Captured image to {out_path}")
        return True
    except Exception as e:
        print("Capture failed:", e)
        return False

def predict_disease_tflite(image_path):
    """Predict disease using TFLite model."""
    img = kimage.load_img(image_path, target_size=IMAGE_SIZE)
    arr = kimage.img_to_array(img)
    arr = np.expand_dims(arr, axis=0).astype(np.float32)
    interpreter.set_tensor(input_details[0]['index'], arr)
    interpreter.invoke()
    preds = interpreter.get_tensor(output_details[0]['index'])[0]
    idx = int(np.argmax(preds))
    conf = float(preds[idx]) * 100.0
    label = CLASS_NAMES[idx] if idx < len(CLASS_NAMES) else f"class_{idx}"
    return label, conf, preds

def send_sms_via_sim800(sim_serial, number, message, timeout=10):
    """Send SMS via SIM800 using AT commands."""
    def write(cmd, wait=0.5):
        sim_serial.write((cmd + "\r\n").encode())
        time.sleep(wait)
        resp = b""
        t0 = time.time()
        while time.time() - t0 < 1.0:
            if sim_serial.in_waiting:
                resp += sim_serial.read(sim_serial.in_waiting)
            else:
                time.sleep(0.05)
        return resp.decode(errors="ignore")

    try:
        print(f"Sending SMS to {number}...")
        sim_serial.reset_input_buffer()
        sim_serial.reset_output_buffer()
        r = write("AT", wait=0.5)
        write("ATE0", wait=0.2)
        write("AT+CMGF=1", wait=0.5)
        cmd = f'AT+CMGS="{number}"'
        sim_serial.write((cmd + "\r\n").encode())
        time.sleep(0.5)
        t0 = time.time()
        prompt_seen = False
        while time.time() - t0 < 5.0:
            if sim_serial.in_waiting:
                out = sim_serial.read(sim_serial.in_waiting).decode(errors="ignore")
                if '>' in out:
                    prompt_seen = True
                    break
            time.sleep(0.1)
        if not prompt_seen:
            print("No '>' prompt from SIM800, continuing anyway...")
        sim_serial.write(message.encode())
        sim_serial.write(bytes([26]))  # Ctrl+Z
        t0 = time.time()
        final = ""
        while time.time() - t0 < timeout:
            if sim_serial.in_waiting:
                final += sim_serial.read(sim_serial.in_waiting).decode(errors="ignore")
                if "+CMGS" in final or "ERROR" in final:
                    break
            else:
                time.sleep(0.2)
        return "+CMGS" in final
    except Exception as e:
        print("SIM800 send error:", e)
        return False

def log_detection(plant_id, label, conf, image_file):
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    with open(LOG_FILE, mode='a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([ts, plant_id, label, f"{conf:.2f}", image_file])
    print(f"Logged: {plant_id}, {label}, {conf:.2f}%")

# === Main Loop ===
def main_loop():
    print("Opening Arduino serial on", ARDUINO_PORT)
    arduino = serial.Serial(ARDUINO_PORT, ARDUINO_BAUD, timeout=1)
    time.sleep(2)
    print("Opening SIM800 serial on", SIM800_PORT)
    sim = serial.Serial(SIM800_PORT, SIM800_BAUD, timeout=1)
    time.sleep(1)

    print("Ready. Listening for PlantID lines from Arduino...")
    while True:
        try:
            line = arduino.readline().decode(errors="ignore").strip()
            if not line:
                time.sleep(0.1)
                continue
            print("Received from Arduino:", line)
            plant_id = "".join(ch for ch in line if ch.isalnum() or ch in "-_")
            if not plant_id:
                print("Invalid PlantID, skipping.")
                continue

            out_fn = f"plant_{plant_id}-day1.jpg"
            out_path = os.path.join(IMAGE_DIR, out_fn)
            ok = capture_image_for_plant(plant_id, out_path)
            if not ok:
                print("Capture failed; skipping this PlantID.")
                continue

            label, conf, raw = predict_disease_tflite(out_path)
            print(f"Prediction: {label} ({conf:.2f}%) raw={raw}")

            # Send SMS to farmer
            msg = f"{plant_id}-{label}"
            sent_farmer = send_sms_via_sim800(sim, TARGET_NUMBER, msg)
            if sent_farmer:
                print(f"SMS sent to farmer: {msg} -> {TARGET_NUMBER}")
            else:
                print("SMS failed to send to farmer.")

            # Send SMS to Control Unit
            sent_cu = send_sms_via_sim800(sim, CU_NUMBER, msg)
            if sent_cu:
                print(f"SMS sent to Control Unit: {msg} -> {CU_NUMBER}")
            else:
                print("SMS failed to send to Control Unit.")

            # Log detection
            log_detection(plant_id, label, conf, out_fn)

            # Notify Arduino to move next
            arduino.write(b"DONE\n")
            print(f"Sent DONE to Arduino for {plant_id}")

            time.sleep(1)

        except KeyboardInterrupt:
            print("Interrupted by user, exiting.")
            break
        except Exception as e:
            print("Main loop exception:", e)
            time.sleep(1)

if __name__ == "__main__":
    main_loop()
