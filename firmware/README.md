# Firmware — ESP32 Smart Water Management System

This folder contains the ESP32 firmware written in C++ using the Arduino framework, managed by PlatformIO.

---

## Prerequisites

- VS Code with the PlatformIO IDE extension installed
- PlatformIO downloads the ESP32 toolchain and all libraries automatically on first build

Install the PlatformIO extension in VS Code by searching for "PlatformIO IDE" in the Extensions panel (Ctrl+Shift+X).

---

## Folder structure

```
firmware/
├── include/
│   ├── config.h                    Central configuration — WiFi, MQTT, GPIO pins, alert thresholds
│   └── sensors/
│       ├── waterLevel.h            JSN-SR04T ultrasonic sensor declarations
│       ├── phSensor.h              pH sensor declarations and calibration interface
│       ├── turbiditySensor.h       DIY LDR turbidity sensor declarations
│       ├── flowSensor.h            YF-S201 flow meter declarations and ISR
│       └── leakSensor.h            FC-37 moisture sensor declarations
├── src/
│   ├── main.cpp                    Entry point — setup(), loop(), WiFi, MQTT, alerts
│   └── sensors/
│       ├── waterLevel.cpp          Ultrasonic distance to water level percentage conversion
│       ├── phSensor.cpp            ADC read, two-point calibration, temperature compensation
│       ├── turbiditySensor.cpp     LDR ADC read, voltage to NTU conversion
│       ├── flowSensor.cpp          Interrupt-driven pulse counting, flow rate and total volume
│       └── leakSensor.cpp          Debounced digital read with consecutive-read confirmation
├── test/                           Unit tests 
└── platformio.ini                  PlatformIO project configuration
```

---

## First time setup

1. Open VS Code
2. Click File → Open Folder → select this `firmware/` folder
3. PlatformIO will detect the project automatically
4. Open `include/config.h` and update the WiFi credentials:

```cpp
#define WIFI_SSID     "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"
```

5. Press the build button (checkmark icon in the bottom status bar) or run PlatformIO: Build from the command palette (Ctrl+Shift+P)
6. A successful build shows `[SUCCESS]` in the terminal

---

## Running without hardware (mock mode)

The firmware includes a mock data mode that simulates all sensor readings without any physical hardware. This allows full development and testing of the MQTT pipeline, Firebase integration, and Flutter app without an ESP32 board.

Mock mode is controlled by a single line in `main.cpp`:

```cpp
#define USE_MOCK_DATA true
```

When this is `true`, the firmware publishes simulated sensor readings every 30 seconds. The simulated tank gradually drains from 95% to 8% then refills, pH drifts slightly around 7.0, and all other values remain at realistic steady-state values.

To run in mock mode without a physical board, you do not need to upload anything. Instead, run `backend/simulate.js` on your laptop — it does the same thing as the ESP32 firmware in mock mode, publishing identical JSON payloads to the MQTT broker.

---

## Uploading to the ESP32

1. Connect the ESP32 to your laptop via USB
2. Wait for the driver to recognise the device (a COM port or /dev/tty port appears)
3. Press the upload button (right arrow icon in the bottom status bar) or run PlatformIO: Upload
4. If the upload fails with "Failed to connect", hold the BOOT button on the ESP32 board while the upload starts, then release once you see "Connecting..." in the terminal
5. Open the Serial Monitor (plug icon in the bottom status bar) at 115200 baud

A successful boot prints:

```
╔════════════════════════════════════════╗
║  SmAart Water Management System        ║
║  ESP32 Firmware  |  Booting...         ║
╚════════════════════════════════════════╝
[BOOT] Relay init — valve CLOSED
[BOOT] LEDs init
...
[WiFi] Connected — IP: 192.168.x.x  RSSI: -52 dBm
[MQTT] Connected — subscribed to devices/node_01/commands
[BOOT] Boot complete. Entering main loop.
```

---

## Configuration reference

All configuration is in `include/config.h`. These are the values you will need to update:

**WiFi credentials** — set before uploading:
```cpp
#define WIFI_SSID        "YourNetworkName"
#define WIFI_PASSWORD    "YourPassword"
```

**Tank dimensions** — measure your actual tank:
```cpp
#define TANK_HEIGHT_CM   100.0   // total usable water height in centimetres
#define SENSOR_OFFSET_CM 5.0     // distance from sensor face to 100% full water level
```

**MQTT broker** — default is HiveMQ public broker for development. For production, replace with a private broker:
```cpp
#define MQTT_BROKER    "broker.hivemq.com"
#define MQTT_PORT      1883
#define MQTT_CLIENT_ID "water_node_01"
```

**Alert thresholds** — these match BIS IS 10500:2012 standard. 
```cpp
#define LEVEL_CUTOFF_HIGH   95.0   // auto-close valve above this level
#define LEVEL_WARN_LOW      20.0   // low water warning
#define PH_CRITICAL_LOW     6.5    // BIS IS 10500:2012 lower limit
#define PH_CRITICAL_HIGH    8.5    // BIS IS 10500:2012 upper limit
#define TURBIDITY_CRITICAL  5.0    // BIS IS 10500:2012 limit in NTU
```

---

## GPIO pin assignment

| GPIO | Connected to | Notes |
|---|---|---|
| GPIO34 | pH sensor signal | ADC1, input-only, 10k+20k voltage divider |
| GPIO35 | Turbidity sensor (LDR) | ADC1, input-only, 10k+20k voltage divider |
| GPIO36 | Battery voltage monitor | ADC1, input-only, 100k+100k voltage divider |
| GPIO25 | JSN-SR04T Echo | Digital only — do not analogRead() this pin |
| GPIO26 | JSN-SR04T Trigger | 20 microsecond pulse required (not 10) |
| GPIO27 | YF-S201 flow meter | Hardware interrupt, RISING edge |
| GPIO32 | FC-37 leak sensor | Digital input |
| GPIO4  | DS18B20 temperature | 1-Wire, requires 4.7k pull-up to 3.3V |
| GPIO33 | Relay (valve control) | HIGH = valve closed, LOW = valve open |
| GPIO16 | Red status LED | 330 ohm series resistor |
| GPIO17 | Green status LED | 330 ohm series resistor |
| GPIO5  | Yellow status LED | 330 ohm series resistor, boot strapping safe |

---

## Sensor calibration (lab phase)

Two sensors require calibration with reference solutions before their readings are meaningful.

**pH sensor calibration** is done in `src/sensors/phSensor.cpp`. In the lab, dip the probe in pH 4.0 buffer solution and record the raw voltage from `readPH_rawVoltage()`, then repeat with pH 7.0 buffer solution. Update these two lines:

```cpp
static float cal_voltage_pH4 = 3.07;  // replace with your measured value
static float cal_voltage_pH7 = 2.52;  // replace with your measured value
```

**Turbidity sensor calibration** is done in `src/sensors/turbiditySensor.cpp`. Record the voltage in clean distilled water (0 NTU reference) and in a turbid water sample of known concentration. Update:

```cpp
static float cal_voltage_clean = 3.10;  // replace with your clean water voltage
static float cal_voltage_dirty = 1.80;  // replace with your turbid sample voltage
static float cal_NTU_dirty     = 500.0; // replace with your reference NTU value
```

---

## Switching to real hardware

When hardware is available, two changes are needed and nothing else:

1. In `main.cpp`, change:
```cpp
#define USE_MOCK_DATA true
```
to:
```cpp
#define USE_MOCK_DATA false
```

2. In `include/config.h`, update WiFi credentials, tank height, and sensor offset with your actual measured values.

All sensor code, MQTT logic, alert checking, and valve control work identically in both modes. The mock flag only controls which values are fed into the data pipeline.

---

## Libraries used

| Library | Version | Purpose |
|---|---|---|
| PubSubClient | 2.8 | MQTT publish and subscribe |
| ArduinoJson | 7.x | JSON serialisation for MQTT payloads |
| OneWire | 2.3 | 1-Wire communication for DS18B20 |
| DallasTemperature | 4.0 | DS18B20 temperature reading |
| WiFi | built-in | ESP32 WiFi connection |

All libraries are declared in `platformio.ini` and downloaded automatically by PlatformIO on first build. Do not install them manually.

---

## MQTT payload format

The firmware publishes to topic `devices/node_01/readings` every 30 seconds. The JSON payload format is:

```json
{
  "device_id":     "water_node_01",
  "fw_version":    "1.0.0",
  "mock":          true,
  "level_pct":     72.1,
  "ph":            7.08,
  "turbidity_ntu": 0.8,
  "flow_lpm":      0.0,
  "total_litres":  12.4,
  "leak_detected": false,
  "battery_v":     3.85,
  "valve_open":    false
}
```

A value of `-1` for any numeric field indicates a sensor read error. The backend ignores `-1` values rather than storing them as real readings.

The firmware subscribes to `devices/node_01/commands` for valve control. Valid incoming messages:

```json
{ "command": "OPEN" }
{ "command": "CLOSE" }
```