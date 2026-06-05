# Smart Home-Water Management System

An IoT-based smart water management system for domestic and commercial use. The system continuously monitors water level, water quality, and pipe leakage, then sends real-time alerts and enables automated valve control through a mobile application.


---

## What the system does

A sensor module installed on a water tank measures five parameters simultaneously: water level (percentage of tank capacity), pH of stored water, turbidity, flow rate through the supply pipe, and moisture at the tank base for leak detection. An ESP32 microcontroller reads all sensors, runs threshold logic locally, controls a solenoid valve automatically when the tank is full, and transmits all data to Firebase over WiFi using MQTT protocol. A Flutter mobile app displays a live dashboard, historical graphs, alert history, and valve control.

The self-charging circuit uses a solar panel and 18650 lithium cell so the sensor node requires no mains power connection.

---

## Repository structure

```
smAart-water-system/
├── firmware/        ESP32 firmware written in C++ using Arduino framework (PlatformIO)
├── backend/         Node.js backend — MQTT bridge, Firebase writer, alert engine, REST API
├── app/             Flutter mobile application (Android primary)
```

---

## Hardware

The system is built around an ESP32 DevKit V1 (30-pin, DOIT). Key components:

- JSN-SR04T waterproof ultrasonic sensor for water level
- pH test strips with TCS34725 colour sensor
- DIY turbidity sensor using LDR and bright white LED
- YF-S201 hall-effect flow meter
- FC-37 resistive moisture sensor for leak detection
- DS18B20 waterproof temperature probe for pH temperature compensation
- 12V normally-closed solenoid valve with opto-isolated relay module
- TP4056 lithium charging module, 18650 cell, and 5V 1W solar panel for self-charging


---

## Software stack

| Layer | Technology |
|---|---|
| Firmware | C++ / Arduino framework, PlatformIO |
| Communication | MQTT over WiFi (HiveMQ public broker for development) |
| Backend | Node.js, Firebase Admin SDK, Express |
| Database | Firebase Firestore (time-series sensor readings) |
| Notifications | Firebase Cloud Messaging (FCM) |
| Mobile app | Flutter (Dart), Firebase SDK |

---

## Setup instructions

Each subfolder has its own README with detailed setup steps. Brief summary:

**Firmware** — requires VS Code with PlatformIO extension. See `firmware/README.md`.

**Backend** — requires Node.js 18 or above. Requires a Firebase service account key. See `backend/README.md`.

**App** — requires Flutter SDK 3.x and Android Studio or Xcode. Requires FlutterFire CLI for Firebase configuration. See `app/README.md`.

---
