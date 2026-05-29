// include/config.h
// ─────────────────────────────────────────────────────────────
// Central configuration: GPIO pins, thresholds, timing constants
// All values match GPIO Pin Assignment Document v1.0
// ─────────────────────────────────────────────────────────────
#ifndef CONFIG_H
#define CONFIG_H

// ── WiFi credentials ─────────────────────────────────────────
#define WIFI_SSID        "Galaxy M33 5G 9DDE"
#define WIFI_PASSWORD    "qeeq9644"

// ── MQTT broker ──────────────────────────────────────────────
#define MQTT_BROKER      "broker.hivemq.com"   // free public broker for testing
#define MQTT_PORT        1883
#define MQTT_CLIENT_ID   "water_node_01"
#define MQTT_TOPIC_PUB   "devices/node_01/readings"
#define MQTT_TOPIC_SUB   "devices/node_01/commands"

// ── GPIO Pin Assignments (matches GPIO doc v1.0) ─────────────
// Analog sensors — ADC1 ONLY (WiFi-safe)
#define PIN_PH_SENSOR       34
#define PIN_TURBIDITY       35
#define PIN_BATTERY_MON     36

// Digital sensors
#define PIN_ULTRASONIC_TRIG 26
#define PIN_ULTRASONIC_ECHO 25
#define PIN_FLOW_METER      27
#define PIN_LEAK_SENSOR     32
#define PIN_TEMP_SENSOR     4   // DS18B20 one-wire

// Outputs
#define PIN_RELAY           33
#define PIN_LED_RED         16
#define PIN_LED_GREEN       17
#define PIN_LED_YELLOW      5

// ── Tank physical dimensions ──────────────────────────────────
// Measure your actual tank and update these values
#define TANK_HEIGHT_CM      100.0  // Total usable height in cm
#define SENSOR_OFFSET_CM    5.0    // Distance from sensor face to 100% full water level

// ── Alert thresholds (matches SRS Section 4) ─────────────────
#define LEVEL_CUTOFF_HIGH   95.0   // % — close valve
#define LEVEL_WARN_LOW      20.0   // % — low water warning
#define LEVEL_CRITICAL_LOW  10.0   // % — critical low

#define PH_WARN_LOW         6.8
#define PH_WARN_HIGH        8.2
#define PH_CRITICAL_LOW     6.5    // BIS IS 10500:2012 limit
#define PH_CRITICAL_HIGH    8.5    // BIS IS 10500:2012 limit

#define TURBIDITY_WARN      1.0    // NTU
#define TURBIDITY_CRITICAL  5.0    // NTU — BIS IS 10500:2012 limit

#define FLOW_LEAK_LPM       0.5    // L/min — flow above this at night = leak
#define LEAK_NIGHT_START    1      // hour (24h) — start of zero-usage period
#define LEAK_NIGHT_END      4      // hour (24h) — end of zero-usage period

// ── Timing ────────────────────────────────────────────────────
#define READING_INTERVAL_MS     30000   // sensor read every 30 seconds
#define PH_INTERVAL_MS          60000   // pH read every 60 seconds
#define TURBIDITY_INTERVAL_MS   60000   // turbidity read every 60 seconds

// ── ADC calibration ───────────────────────────────────────────
#define ADC_RESOLUTION      4095.0  // 12-bit ADC (0–4095)
#define ADC_VREF            3.3     // ESP32 reference voltage

#endif // CONFIG_H