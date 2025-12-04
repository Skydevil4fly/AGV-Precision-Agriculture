# 🌱 Agribot — Smart Farming Automation System

Agribot is an IoT-based smart agriculture system designed to help farmers monitor plant health and automatically deliver treatment when required. It uses a TensorFlow Lite model running on a Raspberry Pi, sends SMS alerts to the farmer, and controls an Arduino-based unit to deliver medicine or nutrients to specific plants.

> 🚧 **Project Status: Work in Progress**  
> Core logic (ML model, SMS alerts, Raspberry Pi → Arduino communication, and dosing mechanism) is functional. Future updates will include autonomous navigation and field deployment.

---

## 🔧 System Overview

Agribot consists of two main modules:

| Module | Role |
|--------|------|
| **Raspberry Pi** | Runs machine learning model, sends SMS, communicates with Arduino |
| **Arduino Control Unit** | Controls pumps/valves to deliver medicine based on commands |
| *(Future)* Line-Following Rover | Autonomous movement and path navigation (planned) |

---

## ✨ Features

- 🧠 TensorFlow Lite plant disease detection  
- 📩 Automatic SMS updates to farmer  
- 💧 Targeted medicine delivery system  
- ⚡ Relay / solenoid valve control through Arduino  
- 🔌 Raspberry Pi ↔ Arduino UART communication  
- 🤖 Future upgrade: autonomous line-following rover  

