# 🌱 Agribot — Smart Autonomous Farming System

Agribot is an IoT-based automated plant care project combining **machine learning, robotics, and embedded systems**.  
It can detect plant diseases, alert the farmer via SMS, and remotely deliver fertilizer or medicine to individual plants.  
A line-following robot module is being developed for autonomous farm navigation.

> 🚧 **Project Status: Work in Progress**  
> Machine learning + SMS + Line-following navigation are functional.  
> control unit is partially complete 

---

## 🧠 System Architecture

Agribot consists of three major components:

| Module | Hardware | Purpose |
|--------|----------|---------|
| **Raspberry Pi ML & Control** | Raspberry Pi + Camera + GSM | Runs TFLite model, detects diseases, sends SMS alerts, commands Arduino |
| **Arduino Control Unit (C.U)** | Arduino + Relays/Pump | Receives commands from Pi to deliver fertilizer/medicine to specific plants |
| **Line-Following Robot (Mobility)** | Arduino + IR Sensors + L298N + Motors | Autonomous navigation system *(under development)* |

---

## ✨ Features

- 🧠 TensorFlow Lite inference for disease detection  
- 📩 Automatic SMS alerts to the farmer  
- 💧 Precision medicine delivery to individual plants  
- 🔌 Serial communication between Raspberry Pi and Arduino  
- 🤖 Autonomous line-following movement (prototype)  
- 🧱 Modular structure for future scaling  

---

