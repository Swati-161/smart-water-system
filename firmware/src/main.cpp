// src/main.cpp
// ─────────────────────────────────────────────────────────────────────
// SmAart Home-Water Management System — Main Firmware
// ESP32 DevKit V1  |  PlatformIO + Arduino framework
//
// ── STATUS ───────────────────────────────────────────────────────────
//   ✅ waterLevel.cpp       JSN-SR04T ultrasonic
//   ✅ phSensor.cpp         TCS34725 colour sensor + pH test strips
//   ✅ turbiditySensor.cpp  DIY LDR turbidity sensor
//   ✅ flowSensor.cpp       YF-S201 hall-effect flow meter
//   ✅ leakSensor.cpp       FC-37 moisture sensor
//
// ── MOCK MODE ────────────────────────────────────────────────────────
//   USE_MOCK_DATA true  → simulated values (no hardware needed)
//   USE_MOCK_DATA false → real sensor functions (hardware required)
//
// ── WHEN HARDWARE ARRIVES — only 2 changes needed ────────────────────
//   1. config.h  → update TANK_HEIGHT_CM after measuring real tank
//   2. main.cpp  → set USE_MOCK_DATA to false
// ─────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensors/waterLevel.h"
#include "sensors/phSensor.h"
#include "sensors/turbiditySensor.h"
#include "sensors/flowSensor.h"
#include "sensors/leakSensor.h"

// ─────────────────────────────────────────────────────────────────────
// ▶▶ SINGLE SWITCH — flip to false when hardware is connected ◀◀
// ─────────────────────────────────────────────────────────────────────
#define USE_MOCK_DATA true

// ─────────────────────────────────────────────────────────────────────
// SENSOR DATA STRUCT
// ─────────────────────────────────────────────────────────────────────
struct SensorData {
    float level;          // % (0.0–100.0),  -1.0 = error
    float pH;             // pH units,        -1.0 = error
    float turbidity;      // NTU,             -1.0 = error
    float flowRate;       // L/min,           -1.0 = error
    float totalLitres;    // cumulative litres today
    bool  leakDetected;   // true = moisture detected
    float batteryVoltage; // V (3.0–4.2)
    bool  mockActive;     // true = mock data in use
};

// ─────────────────────────────────────────────────────────────────────
// MOCK DATA
// ─────────────────────────────────────────────────────────────────────
namespace Mock {
    float level     = 72.0;
    float pH        = 7.1;
    float turbidity = 0.8;
    float flow      = 0.0;
    float litres    = 0.0;
    bool  leak      = false;
    float battery   = 3.85;

    void tick() {
        level -= 0.4;
        if (level < 8.0) level = 95.0;
        pH      = 7.0 + (random(-15, 20) / 100.0);
        litres += 0.2;
        battery -= 0.0005;
        if (battery < 3.2) battery = 4.1;
    }
}

// ─────────────────────────────────────────────────────────────────────
// WIFI + MQTT
// ─────────────────────────────────────────────────────────────────────
WiFiClient   espClient;
PubSubClient mqtt(espClient);

static bool wifiConnected = false;
static bool mqttConnected = false;

// Forward declarations
void connectWiFi();
void connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishSensorData(const SensorData& data);
String buildPayload(const SensorData& data);
void handleCommand(const String& command);
void checkAlerts(const SensorData& data);
void updateLEDs(const SensorData& data);
void printReadings(const SensorData& data);
SensorData readAllSensors();

// ─────────────────────────────────────────────────────────────────────
// TIMING
// ─────────────────────────────────────────────────────────────────────
static unsigned long lastSensorRead = 0;
static unsigned long lastMqttRetry  = 0;
static unsigned long lastWifiRetry  = 0;
static const unsigned long MQTT_RETRY_INTERVAL = 10000;
static const unsigned long WIFI_RETRY_INTERVAL = 30000;

// ─────────────────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n╔════════════════════════════════════════╗");
    Serial.println(  "║  SmAart Water Management System        ║");
    Serial.println(  "║  ESP32 Firmware  |  Booting...         ║");
    Serial.println(  "╚════════════════════════════════════════╝");

    // ── STEP 1: Relay — MUST be first ────────────────────────────────
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, HIGH);
    Serial.println("[BOOT] Relay init — valve CLOSED");

    // ── STEP 2: LEDs ──────────────────────────────────────────────────
    pinMode(PIN_LED_GREEN,  OUTPUT);
    pinMode(PIN_LED_RED,    OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    digitalWrite(PIN_LED_GREEN,  LOW);
    digitalWrite(PIN_LED_RED,    LOW);
    digitalWrite(PIN_LED_YELLOW, HIGH);
    Serial.println("[BOOT] LEDs init");

    // ── STEP 3: Sensors ───────────────────────────────────────────────
    #if USE_MOCK_DATA
        Serial.println("[BOOT] MOCK DATA MODE — no hardware required");
    #else
        waterLevel_init();
        leakSensor_init();
        flowSensor_init();
        turbiditySensor_init();
        phSensor_init();    // TCS34725 via I2C — init last
        Serial.println("[BOOT] All sensors initialised");
    #endif

    // ── STEP 4: WiFi ──────────────────────────────────────────────────
    connectWiFi();

    // ── STEP 5: MQTT ──────────────────────────────────────────────────
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqtt.setKeepAlive(60);
    mqtt.setBufferSize(512);
    if (wifiConnected) connectMQTT();

    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_GREEN,  wifiConnected && mqttConnected ? HIGH : LOW);

    #if USE_MOCK_DATA
        Serial.println("[BOOT] Running in MOCK mode");
    #else
        Serial.println("[BOOT] Running with REAL hardware");
    #endif

    Serial.println("[BOOT] Boot complete.");
    Serial.println("─────────────────────────────────────────────");

    // ── Print available serial commands ──────────────────────────────
    Serial.println("[CMD] Serial commands available (real hardware mode only):");
    Serial.println("[CMD]   RAWPH          — print raw R,G,B,C from colour sensor");
    Serial.println("[CMD]   RAWTB          — print raw turbidity voltage");
    Serial.println("[CMD]   CAL4 R G B     — calibrate pH4 with RGB values");
    Serial.println("[CMD]   CAL7 R G B     — calibrate pH7 with RGB values");
    Serial.println("[CMD]   CAL10 R G B    — calibrate pH10 with RGB values");
    Serial.println("[CMD]   VALVE OPEN     — open solenoid valve");
    Serial.println("[CMD]   VALVE CLOSE    — close solenoid valve");
    Serial.println("─────────────────────────────────────────────");
}

// ─────────────────────────────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────────────────────────────
void loop() {
    unsigned long now = millis();

    // ── Serial calibration command handler ────────────────────────────
    // These commands are only meaningful when real hardware is connected.
    // In mock mode they do nothing harmful — they just won't return
    // real sensor values.
    //
    // Usage in Serial Monitor (set line ending to "Newline"):
    //   RAWPH          → prints raw R,G,B,C from TCS34725
    //   RAWTB          → prints raw turbidity voltage from LDR
    //   CAL4 180 80 60 → sets pH 4.0 calibration to those RGB values
    //   CAL7 120 140 80
    //   CAL10 60 80 160
    //   VALVE OPEN     → opens solenoid valve immediately
    //   VALVE CLOSE    → closes solenoid valve immediately
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "RAWPH") {
            uint16_t r, g, b, c;
            readPH_rawColour(r, g, b, c);
            Serial.println("[CMD] R:" + String(r) + " G:" + String(g)
                + " B:" + String(b) + " C:" + String(c));

        } else if (cmd == "RAWTB") {
            Serial.println("[CMD] Turbidity voltage: "
                + String(readTurbidity_rawVoltage(), 4) + "V");

        } else if (cmd.startsWith("CAL4 ") || cmd.startsWith("CAL7 ")
                || cmd.startsWith("CAL10 ")) {
            // Parse: CAL4 R G B  or  CAL10 R G B
            float targetPH;
            int spaceAfterCmd;
            if (cmd.startsWith("CAL10 ")) {
                targetPH = 10.0;
                spaceAfterCmd = 6;
            } else if (cmd.startsWith("CAL4 ")) {
                targetPH = 4.0;
                spaceAfterCmd = 5;
            } else {
                targetPH = 7.0;
                spaceAfterCmd = 5;
            }

            String args = cmd.substring(spaceAfterCmd);
            // Expected format: "180 80 60"
            int s1 = args.indexOf(' ');
            int s2 = args.indexOf(' ', s1 + 1);
            if (s1 < 0 || s2 < 0) {
                Serial.println("[CMD] Format error. Use: CAL4 R G B (e.g. CAL4 180 80 60)");
            } else {
                uint16_t r = args.substring(0, s1).toInt();
                uint16_t g = args.substring(s1 + 1, s2).toInt();
                uint16_t b = args.substring(s2 + 1).toInt();
                phSensor_calibrate(targetPH, r, g, b);
            }

        } else if (cmd == "VALVE OPEN") {
            handleCommand("OPEN");

        } else if (cmd == "VALVE CLOSE") {
            handleCommand("CLOSE");

        } else if (cmd.length() > 0) {
            Serial.println("[CMD] Unknown command: " + cmd);
            Serial.println("[CMD] Type RAWPH, RAWTB, CAL4/7/10 R G B, VALVE OPEN/CLOSE");
        }
    }

    // ── Maintain WiFi ─────────────────────────────────────────────────
    if (WiFi.status() != WL_CONNECTED) {
        wifiConnected = false;
        if (now - lastWifiRetry >= WIFI_RETRY_INTERVAL) {
            lastWifiRetry = now;
            Serial.println("[WiFi] Disconnected — attempting reconnect...");
            connectWiFi();
        }
    }

    // ── Maintain MQTT ─────────────────────────────────────────────────
    if (wifiConnected && !mqtt.connected()) {
        mqttConnected = false;
        if (now - lastMqttRetry >= MQTT_RETRY_INTERVAL) {
            lastMqttRetry = now;
            connectMQTT();
        }
    }
    if (mqttConnected) mqtt.loop();

    // ── Read sensors and publish ──────────────────────────────────────
    if (now - lastSensorRead >= READING_INTERVAL_MS) {
        lastSensorRead = now;

        SensorData data = readAllSensors();
        printReadings(data);
        checkAlerts(data);
        updateLEDs(data);

        if (mqttConnected) {
            publishSensorData(data);
        } else {
            Serial.println("[MQTT] Not connected — data not published");
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
// READ ALL SENSORS
// ─────────────────────────────────────────────────────────────────────
SensorData readAllSensors() {
    SensorData data;

    #if USE_MOCK_DATA
        Mock::tick();
        data.mockActive     = true;
        data.level          = Mock::level;
        data.pH             = Mock::pH;
        data.turbidity      = Mock::turbidity;
        data.flowRate       = Mock::flow;
        data.totalLitres    = Mock::litres;
        data.leakDetected   = Mock::leak;
        data.batteryVoltage = Mock::battery;

    #else
        data.mockActive = false;

        // Water level — JSN-SR04T via GPIO25/26
        data.level = readWaterLevel();

        // pH — TCS34725 colour sensor + pH test strips via I2C (GPIO21/22)
        data.pH = readPH();

        // Turbidity — DIY LDR sensor via GPIO35
        data.turbidity = readTurbidity();

        // Flow rate — YF-S201 via GPIO27 interrupt
        data.flowRate    = readFlowRate();
        data.totalLitres = getTotalLitres();

        // Leak — FC-37 via GPIO32
        data.leakDetected = checkLeakSensor();

        // Battery voltage — mocked until PCB assembled
        // After PCB: comment line below, uncomment the two lines after it
        data.batteryVoltage = 3.85;
        // float raw = analogRead(PIN_BATTERY_MON);
        // data.batteryVoltage = (raw / ADC_RESOLUTION) * ADC_VREF * 2.0;
    #endif

    return data;
}

// ─────────────────────────────────────────────────────────────────────
// WIFI CONNECTION
// ─────────────────────────────────────────────────────────────────────
void connectWiFi() {
    Serial.print("[WiFi] Connecting to: ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\n[WiFi] Connected — IP: " + WiFi.localIP().toString()
            + "  RSSI: " + String(WiFi.RSSI()) + " dBm");
        digitalWrite(PIN_LED_GREEN, HIGH);
    } else {
        wifiConnected = false;
        Serial.println("\n[WiFi] Failed — will retry in 30s");
    }
}

// ─────────────────────────────────────────────────────────────────────
// MQTT CONNECTION
// ─────────────────────────────────────────────────────────────────────
void connectMQTT() {
    Serial.print("[MQTT] Connecting to: ");
    Serial.println(MQTT_BROKER);

    String lwtTopic = String("devices/") + MQTT_CLIENT_ID + "/status";
    String lwtMsg   = "{\"status\":\"offline\",\"device\":\""
                    + String(MQTT_CLIENT_ID) + "\"}";

    bool ok = mqtt.connect(
        MQTT_CLIENT_ID,
        nullptr, nullptr,
        lwtTopic.c_str(), 1,
        true,
        lwtMsg.c_str()
    );

    if (ok) {
        mqttConnected = true;
        mqtt.subscribe(MQTT_TOPIC_SUB);
        Serial.println("[MQTT] Connected — subscribed to " + String(MQTT_TOPIC_SUB));
        String onlineMsg = "{\"status\":\"online\",\"device\":\""
                         + String(MQTT_CLIENT_ID) + "\",\"fw\":\"1.0.0\"}";
        mqtt.publish(lwtTopic.c_str(), onlineMsg.c_str(), true);
    } else {
        mqttConnected = false;
        Serial.println("[MQTT] Failed — state: " + String(mqtt.state()));
    }
}

// ─────────────────────────────────────────────────────────────────────
// MQTT CALLBACK
// ─────────────────────────────────────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println("[MQTT] Received [" + String(topic) + "]: " + message);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, message);
    if (err) {
        Serial.println("[MQTT] JSON parse error: " + String(err.c_str()));
        return;
    }

    String command = doc["command"] | "";
    handleCommand(command);
}

// ─────────────────────────────────────────────────────────────────────
// VALVE CONTROL
// ─────────────────────────────────────────────────────────────────────
void handleCommand(const String& command) {
    if (command == "OPEN") {
        digitalWrite(PIN_RELAY, LOW);
        Serial.println("[Valve] Opened");
    } else if (command == "CLOSE") {
        digitalWrite(PIN_RELAY, HIGH);
        Serial.println("[Valve] Closed");
    } else {
        Serial.println("[Valve] Unknown command: " + command);
    }
}

// ─────────────────────────────────────────────────────────────────────
// BUILD JSON PAYLOAD
// ─────────────────────────────────────────────────────────────────────
String buildPayload(const SensorData& data) {
    JsonDocument doc;

    doc["device_id"]     = MQTT_CLIENT_ID;
    doc["fw_version"]    = "1.0.0";
    doc["mock"]          = data.mockActive;
    doc["level_pct"]     = data.level;
    doc["ph"]            = data.pH;
    doc["turbidity_ntu"] = data.turbidity;
    doc["flow_lpm"]      = data.flowRate;
    doc["total_litres"]  = data.totalLitres;
    doc["leak_detected"] = data.leakDetected;
    doc["battery_v"]     = data.batteryVoltage;
    doc["valve_open"]    = (digitalRead(PIN_RELAY) == LOW);

    String payload;
    serializeJson(doc, payload);
    return payload;
}

// ─────────────────────────────────────────────────────────────────────
// PUBLISH SENSOR DATA
// ─────────────────────────────────────────────────────────────────────
void publishSensorData(const SensorData& data) {
    String payload = buildPayload(data);
    if (mqtt.publish(MQTT_TOPIC_PUB, payload.c_str())) {
        Serial.println("[MQTT] Published: " + payload);
    } else {
        Serial.println("[MQTT] Publish failed — length: "
            + String(payload.length()) + " bytes");
    }
}

// ─────────────────────────────────────────────────────────────────────
// ALERT CHECKING
// ─────────────────────────────────────────────────────────────────────
void checkAlerts(const SensorData& data) {
    bool anyAlert = false;

    if (data.level >= 0) {
        if (data.level >= LEVEL_CUTOFF_HIGH) {
            Serial.println("[ALERT] CRITICAL — Tank full ("
                + String(data.level, 1) + "%) — auto closing valve");
            digitalWrite(PIN_RELAY, HIGH);
            anyAlert = true;
        } else if (data.level <= LEVEL_CRITICAL_LOW) {
            Serial.println("[ALERT] CRITICAL — Water critically low ("
                + String(data.level, 1) + "%)");
            anyAlert = true;
        } else if (data.level <= LEVEL_WARN_LOW) {
            Serial.println("[ALERT] WARNING — Water low ("
                + String(data.level, 1) + "%)");
            anyAlert = true;
        }
    }

    if (data.pH >= 0) {
        if (data.pH < PH_CRITICAL_LOW || data.pH > PH_CRITICAL_HIGH) {
            Serial.println("[ALERT] CRITICAL — pH " + String(data.pH, 1)
                + " outside BIS safe range (6.5–8.5)");
            anyAlert = true;
        } else if (data.pH < PH_WARN_LOW || data.pH > PH_WARN_HIGH) {
            Serial.println("[ALERT] WARNING — pH " + String(data.pH, 1)
                + " approaching unsafe range");
            anyAlert = true;
        }
    }

    if (data.turbidity >= 0) {
        if (data.turbidity > TURBIDITY_CRITICAL) {
            Serial.println("[ALERT] CRITICAL — Turbidity "
                + String(data.turbidity, 1) + " NTU exceeds BIS limit");
            anyAlert = true;
        } else if (data.turbidity > TURBIDITY_WARN) {
            Serial.println("[ALERT] WARNING — Turbidity "
                + String(data.turbidity, 1) + " NTU — water becoming cloudy");
            anyAlert = true;
        }
    }

    if (data.leakDetected) {
        Serial.println("[ALERT] CRITICAL — Leak detected at moisture sensor");
        anyAlert = true;
    }

    if (!anyAlert) {
        Serial.println("[ALERT] All parameters within safe range");
    }
}

// ─────────────────────────────────────────────────────────────────────
// LED STATUS INDICATOR
// ─────────────────────────────────────────────────────────────────────
void updateLEDs(const SensorData& data) {
    bool critical = (data.level >= LEVEL_CUTOFF_HIGH)
                 || (data.level <= LEVEL_CRITICAL_LOW  && data.level >= 0)
                 || (data.pH    <  PH_CRITICAL_LOW      && data.pH    >= 0)
                 || (data.pH    >  PH_CRITICAL_HIGH     && data.pH    >= 0)
                 || (data.turbidity > TURBIDITY_CRITICAL && data.turbidity >= 0)
                 || data.leakDetected;

    bool warning  = (data.level <= LEVEL_WARN_LOW       && data.level >= 0)
                 || (data.pH    <  PH_WARN_LOW           && data.pH    >= 0)
                 || (data.pH    >  PH_WARN_HIGH          && data.pH    >= 0)
                 || (data.turbidity > TURBIDITY_WARN     && data.turbidity >= 0)
                 || !wifiConnected;

    if (critical) {
        digitalWrite(PIN_LED_RED,    HIGH);
        digitalWrite(PIN_LED_YELLOW, LOW);
        digitalWrite(PIN_LED_GREEN,  LOW);
    } else if (warning) {
        digitalWrite(PIN_LED_RED,    LOW);
        digitalWrite(PIN_LED_YELLOW, HIGH);
        digitalWrite(PIN_LED_GREEN,  LOW);
    } else {
        digitalWrite(PIN_LED_RED,    LOW);
        digitalWrite(PIN_LED_YELLOW, LOW);
        digitalWrite(PIN_LED_GREEN,  HIGH);
    }
}

// ─────────────────────────────────────────────────────────────────────
// PRINT READINGS TO SERIAL MONITOR
// ─────────────────────────────────────────────────────────────────────
void printReadings(const SensorData& data) {
    Serial.println("┌────────────────────────────────────────────┐");
    Serial.println("│            SENSOR READINGS                  │");
    Serial.println("├────────────────────────────────────────────┤");
    Serial.println("│ Mode        : "
        + String(data.mockActive ? "MOCK (simulated)" : "REAL (hardware)"));
    Serial.println("│ Water Level : "
        + (data.level < 0 ? String("ERROR") : String(data.level, 1) + " %"));
    Serial.println("│ pH          : "
        + (data.pH < 0 ? String("ERROR") : String(data.pH, 1)));
    Serial.println("│ Turbidity   : "
        + (data.turbidity < 0 ? String("ERROR") : String(data.turbidity, 1) + " NTU"));
    Serial.println("│ Flow Rate   : "
        + (data.flowRate < 0 ? String("ERROR") : String(data.flowRate, 2) + " L/min"));
    Serial.println("│ Total Today : " + String(data.totalLitres, 1) + " L");
    Serial.println("│ Leak        : " + String(data.leakDetected ? "DETECTED" : "None"));
    Serial.println("│ Battery     : " + String(data.batteryVoltage, 2) + " V");
    Serial.println("│ WiFi        : " + String(wifiConnected ? "Connected" : "Offline"));
    Serial.println("│ MQTT        : " + String(mqttConnected ? "Connected" : "Offline"));
    Serial.println("└────────────────────────────────────────────┘");
}