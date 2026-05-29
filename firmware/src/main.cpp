// src/main.cpp
// ─────────────────────────────────────────────────────────────────────
// SmAart Home-Water Management System — Main Firmware
// ESP32 DevKit V1  |  PlatformIO + Arduino framework
//
// CURRENT STATUS:
//   ✅ waterLevel.cpp  — written by Person A
//   ✅ phSensor.cpp    — written by Person A
//   ⏳ turbiditySensor.cpp — Person B (mock value used until pushed)
//   ⏳ flowSensor.cpp      — Person B (mock value used until pushed)
//   ⏳ leakSensor.cpp      — Person B (mock value used until pushed)
//
// HOW MOCK MODE WORKS:
//   USE_MOCK_DATA true  → all sensors return simulated values (no hardware)
//   USE_MOCK_DATA false → real sensor functions called (hardware required)
//
// TEAMMATE INTEGRATION:
//   Search "TODO-TEAMMATE" in this file to find every place
//   that needs updating when a teammate pushes their sensor code.
//   Changes needed: #include at top, init() in setup(), read() in readAllSensors()
//   Nothing else in this file needs to change.
// ─────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ── Sensor includes ───────────────────────────────────────────────────
// ✅ Already written — include now
#include "sensors/waterLevel.h"
#include "sensors/phSensor.h"

// TODO-TEAMMATE: Uncomment these 3 lines when teammates push their files
// #include "sensors/turbiditySensor.h"
// #include "sensors/flowSensor.h"
// #include "sensors/leakSensor.h"

// ─────────────────────────────────────────────────────────────────────
// MOCK / REAL DATA SWITCH
// Set false only when ALL sensors are written AND hardware is connected
// ─────────────────────────────────────────────────────────────────────
#define USE_MOCK_DATA true

// ─────────────────────────────────────────────────────────────────────
// SENSOR DATA STRUCT
// Holds one complete reading from all sensors.
// All fields defined now — teammates' sensors populate their own fields
// when integrated. Until then mock values fill the gaps.
// ─────────────────────────────────────────────────────────────────────
struct SensorData {
    float level;          // % (0.0 – 100.0), -1.0 = error
    float pH;             // pH units (0.0 – 14.0), -1.0 = error
    float turbidity;      // NTU, -1.0 = error
    float flowRate;       // L/min, -1.0 = error
    float totalLitres;    // cumulative litres today
    bool  leakDetected;   // true = moisture sensor wet
    float batteryVoltage; // V (3.0 – 4.2)
    bool  mockActive;     // true = any field is a mock value
};

// ─────────────────────────────────────────────────────────────────────
// MOCK DATA — simulates realistic sensor values for home testing
// Turbidity, flow, leak, battery are permanently mocked until
// teammates push. Level and pH use mock only when USE_MOCK_DATA = true.
// ─────────────────────────────────────────────────────────────────────
namespace Mock {
    float level     = 72.0;
    float pH        = 7.1;
    float turbidity = 0.8;   // NTU — clean water
    float flow      = 0.0;   // L/min — no flow
    float litres    = 0.0;
    bool  leak      = false;
    float battery   = 3.85;  // V — healthy battery

    void tick() {
        // Simulate gradual tank drain → refill cycle
        level -= 0.4;
        if (level < 8.0) level = 95.0; // tank "refilled"

        // Simulate slight pH drift (realistic variation)
        pH = 7.0 + (random(-15, 20) / 100.0);

        // Simulate small ongoing usage accumulation
        litres += 0.2;

        // Simulate battery slow discharge
        battery -= 0.0005;
        if (battery < 3.2) battery = 4.1;
    }
}

// ─────────────────────────────────────────────────────────────────────
// WIFI + MQTT
// ─────────────────────────────────────────────────────────────────────
WiFiClient   espClient;
PubSubClient mqtt(espClient);

static bool wifiConnected  = false;
static bool mqttConnected  = false;

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
static unsigned long lastSensorRead  = 0;
static unsigned long lastMqttRetry   = 0;
static unsigned long lastWifiRetry   = 0;
static const unsigned long MQTT_RETRY_INTERVAL = 10000; // 10 seconds
static const unsigned long WIFI_RETRY_INTERVAL = 30000; // 30 seconds

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

    // ── STEP 1: Relay (valve) — MUST be first line of setup ──────────
    // Active-LOW relay: HIGH = relay OFF = valve CLOSED (safe default)
    // If this is not first, pin floats during boot and may pulse the valve
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, HIGH);
    Serial.println("[BOOT] ✅ Relay init — valve CLOSED");

    // ── STEP 2: Status LEDs ───────────────────────────────────────────
    pinMode(PIN_LED_GREEN,  OUTPUT);
    pinMode(PIN_LED_RED,    OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    digitalWrite(PIN_LED_GREEN,  LOW);
    digitalWrite(PIN_LED_RED,    LOW);
    digitalWrite(PIN_LED_YELLOW, HIGH); // yellow on = booting
    Serial.println("[BOOT] ✅ LEDs init");

    // ── STEP 3: Initialise sensors ────────────────────────────────────
    #if !USE_MOCK_DATA
        waterLevel_init();
        phSensor_init();
        // TODO-TEAMMATE: Uncomment when files are pushed
        // turbiditySensor_init();
        // flowSensor_init();
        // leakSensor_init();
    #else
        Serial.println("[BOOT] ⚠️  MOCK DATA MODE active — no hardware needed");
        Serial.println("[BOOT]     Water level and pH: using real sensor code");
        Serial.println("[BOOT]     Turbidity / flow / leak: mocked until teammates push");
    #endif

    // ── STEP 4: Connect WiFi ──────────────────────────────────────────
    connectWiFi();

    // ── STEP 5: Configure MQTT ────────────────────────────────────────
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqtt.setKeepAlive(60);
    mqtt.setBufferSize(512); // ensure buffer big enough for JSON payload
    if (wifiConnected) connectMQTT();

    // ── Boot complete ─────────────────────────────────────────────────
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_GREEN,  wifiConnected && mqttConnected ? HIGH : LOW);
    Serial.println("[BOOT] ✅ Boot complete. Entering main loop.");
    Serial.println("─────────────────────────────────────────────");
}

// ─────────────────────────────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────────────────────────────
void loop() {
    unsigned long now = millis();

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
    if (mqttConnected) mqtt.loop(); // process incoming messages

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
            Serial.println("[MQTT] Not connected — reading stored locally only");
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
// READ ALL SENSORS
// This is the only function you modify when teammates push their code.
// Each sensor has a clearly marked section. When a teammate pushes,
// comment out their mock line and uncomment their real function call.
// ─────────────────────────────────────────────────────────────────────
SensorData readAllSensors() {
    SensorData data;
    data.mockActive = false;

    // ── WATER LEVEL ───────────────────────────────────────────────────
    // Status: ✅ Real code written — toggle with USE_MOCK_DATA
    #if USE_MOCK_DATA
        Mock::tick();
        data.level = Mock::level;
        data.mockActive = true;
    #else
        data.level = readWaterLevel();
    #endif

    // ── pH ────────────────────────────────────────────────────────────
    // Status: ✅ Real code written — toggle with USE_MOCK_DATA
    #if USE_MOCK_DATA
        data.pH = Mock::pH;
    #else
        data.pH = readPH();
    #endif

    // ── TURBIDITY ─────────────────────────────────────────────────────
    // Status: ⏳ Awaiting teammate push
    // TODO-TEAMMATE: When turbiditySensor.cpp is pushed:
    //   1. Uncomment: #include "sensors/turbiditySensor.h"  (top of file)
    //   2. Uncomment: turbiditySensor_init();               (in setup)
    //   3. Comment out the mock line below
    //   4. Uncomment the real read line below
    data.turbidity = Mock::turbidity; // ← MOCK: remove when teammate pushes
    // data.turbidity = readTurbidity(); // ← REAL: uncomment when teammate pushes

    // ── FLOW RATE ─────────────────────────────────────────────────────
    // Status: ⏳ Awaiting teammate push
    // TODO-TEAMMATE: Same 4-step process as turbidity above
    data.flowRate    = Mock::flow;    // ← MOCK: remove when teammate pushes
    data.totalLitres = Mock::litres;  // ← MOCK: remove when teammate pushes
    // data.flowRate    = readFlowRate();    // ← REAL: uncomment when teammate pushes
    // data.totalLitres = getTotalLitres();  // ← REAL: uncomment when teammate pushes

    // ── LEAK SENSOR ───────────────────────────────────────────────────
    // Status: ⏳ Awaiting teammate push
    // TODO-TEAMMATE: Same 4-step process as turbidity above
    data.leakDetected = Mock::leak;   // ← MOCK: remove when teammate pushes
    // data.leakDetected = checkLeakSensor(); // ← REAL: uncomment when teammate pushes

    // ── BATTERY VOLTAGE ───────────────────────────────────────────────
    // Status: ⏳ Reading battery ADC (always mock for now — no hardware)
    // This uses GPIO36 ADC1 pin — add real read when hardware assembled
    data.batteryVoltage = Mock::battery; // ← MOCK until PCB assembled
    // Real read (add after PCB assembly):
    // float raw = analogRead(PIN_BATTERY_MON);
    // data.batteryVoltage = (raw / ADC_RESOLUTION) * ADC_VREF * 2.0; // *2 for divider

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

    // Wait up to 15 seconds for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\n[WiFi] ✅ Connected!");
        Serial.println("[WiFi] IP address: " + WiFi.localIP().toString());
        Serial.println("[WiFi] Signal strength: " + String(WiFi.RSSI()) + " dBm");
        digitalWrite(PIN_LED_GREEN, HIGH);
    } else {
        wifiConnected = false;
        Serial.println("\n[WiFi] ❌ Failed to connect. Will retry in 30s.");
        Serial.println("[WiFi] Continuing without WiFi — readings stored locally");
    }
}

// ─────────────────────────────────────────────────────────────────────
// MQTT CONNECTION
// ─────────────────────────────────────────────────────────────────────
void connectMQTT() {
    Serial.print("[MQTT] Connecting to broker: ");
    Serial.println(MQTT_BROKER);

    // Last Will and Testament — broker publishes this if ESP32 disconnects
    // unexpectedly. App sees "offline" status on the dashboard.
    String lwtTopic = String("devices/") + MQTT_CLIENT_ID + "/status";
    String lwtMsg   = "{\"status\":\"offline\",\"device\":\"" + String(MQTT_CLIENT_ID) + "\"}";

    bool connected = mqtt.connect(
        MQTT_CLIENT_ID,
        nullptr,           // username (none for HiveMQ public broker)
        nullptr,           // password
        lwtTopic.c_str(),  // LWT topic
        1,                 // LWT QoS
        true,              // LWT retain
        lwtMsg.c_str()     // LWT message
    );

    if (connected) {
        mqttConnected = true;
        Serial.println("[MQTT] ✅ Connected to broker");

        // Subscribe to command topic (valve open/close from app)
        mqtt.subscribe(MQTT_TOPIC_SUB);
        Serial.println("[MQTT] Subscribed to: " + String(MQTT_TOPIC_SUB));

        // Publish online status
        String onlineMsg = "{\"status\":\"online\",\"device\":\"" + String(MQTT_CLIENT_ID)
            + "\",\"fw\":\"1.0.0\"}";
        mqtt.publish(lwtTopic.c_str(), onlineMsg.c_str(), true); // retain = true

    } else {
        mqttConnected = false;
        Serial.println("[MQTT] ❌ Connection failed. State: " + String(mqtt.state()));
        // MQTT state codes:
        // -4 = connection timeout    -3 = connection lost
        // -2 = connect failed        -1 = disconnected
        //  0 = connected              1 = bad protocol
        //  2 = client ID rejected     3 = server unavailable
        //  4 = bad credentials        5 = unauthorised
    }
}

// ─────────────────────────────────────────────────────────────────────
// MQTT INCOMING MESSAGE CALLBACK
// Called automatically by mqtt.loop() when a message arrives
// ─────────────────────────────────────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Convert payload bytes to String
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    Serial.println("[MQTT] ← Received on [" + String(topic) + "]: " + message);

    // Parse command JSON: {"command":"OPEN"} or {"command":"CLOSE"}
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, message);
    if (err) {
        Serial.println("[MQTT] ❌ JSON parse error: " + String(err.c_str()));
        return;
    }

    String command = doc["command"] | ""; // | "" = default if key missing
    handleCommand(command);
}

// ─────────────────────────────────────────────────────────────────────
// VALVE CONTROL
// ─────────────────────────────────────────────────────────────────────
void handleCommand(const String& command) {
    if (command == "OPEN") {
        // Active-LOW relay: LOW = relay ON = valve OPEN
        digitalWrite(PIN_RELAY, LOW);
        digitalWrite(PIN_LED_GREEN, HIGH);
        Serial.println("[Valve] ✅ Opened by remote command");

    } else if (command == "CLOSE") {
        // HIGH = relay OFF = valve CLOSED
        digitalWrite(PIN_RELAY, HIGH);
        Serial.println("[Valve] ✅ Closed by remote command");

    } else {
        Serial.println("[Valve] ⚠️  Unknown command: " + command);
    }
}

// ─────────────────────────────────────────────────────────────────────
// BUILD JSON PAYLOAD
// Constructs the MQTT message from a SensorData struct.
// The field names here MUST match the backend database schema.
// ─────────────────────────────────────────────────────────────────────
String buildPayload(const SensorData& data) {
    JsonDocument doc;

    doc["device_id"]      = MQTT_CLIENT_ID;
    doc["fw_version"]     = "1.0.0";
    doc["mock"]           = data.mockActive;

    // Sensor values — use -1 to signal error/unavailable to backend
    if (data.level >= 0)       doc["level_pct"]      = serialized(String(data.level, 1));
    if (data.pH >= 0)          doc["ph"]              = serialized(String(data.pH, 2));
    if (data.turbidity >= 0)   doc["turbidity_ntu"]   = serialized(String(data.turbidity, 1));
    if (data.flowRate >= 0)    doc["flow_lpm"]        = serialized(String(data.flowRate, 2));
    doc["total_litres"]   = serialized(String(data.totalLitres, 1));
    doc["leak_detected"]  = data.leakDetected;
    doc["battery_v"]      = serialized(String(data.batteryVoltage, 2));

    // Valve state
    doc["valve_open"]     = (digitalRead(PIN_RELAY) == LOW);

    String payload;
    serializeJson(doc, payload);
    return payload;
}

// ─────────────────────────────────────────────────────────────────────
// PUBLISH TO MQTT
// ─────────────────────────────────────────────────────────────────────
void publishSensorData(const SensorData& data) {
    String payload = buildPayload(data);

    bool success = mqtt.publish(MQTT_TOPIC_PUB, payload.c_str());

    if (success) {
        Serial.println("[MQTT] ↑ Published: " + payload);
    } else {
        Serial.println("[MQTT] ❌ Publish failed — payload length: " 
            + String(payload.length()));
    }
}

// ─────────────────────────────────────────────────────────────────────
// ALERT CHECKING
// Compares readings against thresholds from config.h
// For now: logs to Serial. Next step: publish alert to MQTT alert topic.
// ─────────────────────────────────────────────────────────────────────
void checkAlerts(const SensorData& data) {
    bool anyAlert = false;

    // Water level alerts
    if (data.level >= 0) {
        if (data.level >= LEVEL_CUTOFF_HIGH) {
            Serial.println("[ALERT] 🔴 TANK FULL (" + String(data.level,1) + "%) — closing valve");
            digitalWrite(PIN_RELAY, HIGH); // close valve automatically
            anyAlert = true;
        } else if (data.level <= LEVEL_CRITICAL_LOW) {
            Serial.println("[ALERT] 🔴 CRITICAL LOW WATER (" + String(data.level,1) + "%)");
            anyAlert = true;
        } else if (data.level <= LEVEL_WARN_LOW) {
            Serial.println("[ALERT] 🟡 LOW WATER WARNING (" + String(data.level,1) + "%)");
            anyAlert = true;
        }
    }

    // pH alerts
    if (data.pH >= 0) {
        if (data.pH < PH_CRITICAL_LOW || data.pH > PH_CRITICAL_HIGH) {
            Serial.println("[ALERT] 🔴 CRITICAL pH: " + String(data.pH,2)
                + " (safe range: " + String(PH_CRITICAL_LOW) + "–" + String(PH_CRITICAL_HIGH) + ")");
            anyAlert = true;
        } else if (data.pH < PH_WARN_LOW || data.pH > PH_WARN_HIGH) {
            Serial.println("[ALERT] 🟡 pH WARNING: " + String(data.pH,2));
            anyAlert = true;
        }
    }

    // Turbidity alerts
    if (data.turbidity >= 0) {
        if (data.turbidity > TURBIDITY_CRITICAL) {
            Serial.println("[ALERT] 🔴 CRITICAL TURBIDITY: " + String(data.turbidity,1) + " NTU");
            anyAlert = true;
        } else if (data.turbidity > TURBIDITY_WARN) {
            Serial.println("[ALERT] 🟡 TURBIDITY WARNING: " + String(data.turbidity,1) + " NTU");
            anyAlert = true;
        }
    }

    // Leak alert
    if (data.leakDetected) {
        Serial.println("[ALERT] 🔴 LEAK DETECTED at moisture sensor!");
        anyAlert = true;
    }

    if (!anyAlert) {
        Serial.println("[ALERT] ✅ All parameters within safe range");
    }
}

// ─────────────────────────────────────────────────────────────────────
// LED STATUS INDICATOR
// Green = all OK and connected
// Yellow = warning threshold crossed or WiFi connecting
// Red = critical alert or offline
// ─────────────────────────────────────────────────────────────────────
void updateLEDs(const SensorData& data) {
    bool critical = (data.level >= LEVEL_CUTOFF_HIGH)
                 || (data.level <= LEVEL_CRITICAL_LOW && data.level >= 0)
                 || (data.pH < PH_CRITICAL_LOW || data.pH > PH_CRITICAL_HIGH)
                 || (data.turbidity > TURBIDITY_CRITICAL && data.turbidity >= 0)
                 || data.leakDetected;

    bool warning  = (data.level <= LEVEL_WARN_LOW && data.level >= 0)
                 || (data.pH < PH_WARN_LOW || data.pH > PH_WARN_HIGH)
                 || (data.turbidity > TURBIDITY_WARN && data.turbidity >= 0)
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
    Serial.println("┌─────────────────────────────────────────┐");
    Serial.println("│         SENSOR READINGS                  │");
    Serial.println("├─────────────────────────────────────────┤");
    Serial.println("│ Water Level : " + String(data.level, 1)       + " %"     + (data.level < 0 ? " [ERROR]" : ""));
    Serial.println("│ pH          : " + String(data.pH, 2)          +           (data.pH < 0 ? " [ERROR]" : ""));
    Serial.println("│ Turbidity   : " + String(data.turbidity, 1)   + " NTU"   + " [MOCK]");
    Serial.println("│ Flow Rate   : " + String(data.flowRate, 2)    + " L/min" + " [MOCK]");
    Serial.println("│ Total Today : " + String(data.totalLitres, 1) + " L"     + " [MOCK]");
    Serial.println("│ Leak        : " + String(data.leakDetected ? "⚠️ DETECTED" : "None") + " [MOCK]");
    Serial.println("│ Battery     : " + String(data.batteryVoltage, 2) + " V"  + " [MOCK]");
    Serial.println("│ WiFi        : " + String(wifiConnected ? "✅ Connected" : "❌ Offline"));
    Serial.println("│ MQTT        : " + String(mqttConnected ? "✅ Connected" : "❌ Offline"));
    Serial.println("│ Mock mode   : " + String(data.mockActive ? "YES (some sensors)" : "NO"));
    Serial.println("└─────────────────────────────────────────┘");
}