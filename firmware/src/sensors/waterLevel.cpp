// src/sensors/waterLevel.cpp
// ─────────────────────────────────────────────────────────────
// JSN-SR04T waterproof ultrasonic sensor
// Mounted at top of tank, pointing DOWN at water surface.
// Measures distance from sensor face to water surface.
// Converts distance → water height → percentage.
//
// KEY DIFFERENCE from HC-SR04:
// JSN-SR04T needs a 20µs TRIG pulse (HC-SR04 uses 10µs).
// Using 10µs gives unreliable readings with JSN-SR04T.
// ─────────────────────────────────────────────────────────────
#include "sensors/waterLevel.h"
#include "config.h"
#include <Arduino.h>

void waterLevel_init() {
    pinMode(PIN_ULTRASONIC_TRIG, OUTPUT);
    pinMode(PIN_ULTRASONIC_ECHO, INPUT);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    delay(50); // let sensor settle after power-on
    Serial.println("[WaterLevel] Sensor initialised on TRIG=" 
        + String(PIN_ULTRASONIC_TRIG) + " ECHO=" + String(PIN_ULTRASONIC_ECHO));
}

float readWaterLevelRaw_cm() {
    // Step 1: Send trigger pulse
    // JSN-SR04T requires minimum 20µs HIGH pulse on TRIG
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    delayMicroseconds(4);
    digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(20);  // ← 20µs for JSN-SR04T (not 10µs)
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);

    // Step 2: Measure ECHO pulse duration
    // pulseIn() waits for ECHO to go HIGH, measures how long it stays HIGH
    // Timeout of 30,000µs = max measurable distance ~500cm (more than enough)
    long duration_us = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, 30000);

    // Step 3: Check for timeout (pulseIn returns 0 if no echo received)
    if (duration_us == 0) {
        Serial.println("[WaterLevel] ERROR: No echo received — sensor timeout");
        return -1.0;
    }

    // Step 4: Convert time to distance
    // Sound travels at 343 m/s = 0.0343 cm/µs
    // The pulse travels TO the water and BACK, so divide by 2
    float distance_cm = (duration_us * 0.0343) / 2.0;

    // Step 5: Validate range
    // JSN-SR04T reliable range: 25cm to 450cm
    // Below 25cm is the blind zone (sensor cannot detect objects this close)
    if (distance_cm < 25.0 || distance_cm > 450.0) {
        Serial.println("[WaterLevel] WARNING: Distance " + String(distance_cm) 
            + " cm out of reliable range (25–450cm)");
        return -1.0;
    }

    return distance_cm;
}

float readWaterLevel() {
    // Take 3 readings and average them to reduce noise
    float total = 0;
    int valid = 0;

    for (int i = 0; i < 3; i++) {
        float d = readWaterLevelRaw_cm();
        if (d > 0) {
            total += d;
            valid++;
        }
        delay(60); // wait between readings — sensor needs time to reset
    }

    if (valid == 0) {
        Serial.println("[WaterLevel] ERROR: All 3 readings failed");
        return -1.0;
    }

    float avg_distance_cm = total / valid;

    // Convert distance → water height
    // When tank is FULL: sensor reads SENSOR_OFFSET_CM (small distance)
    // When tank is EMPTY: sensor reads SENSOR_OFFSET_CM + TANK_HEIGHT_CM
    //
    // water_height = TANK_HEIGHT_CM - (distance - SENSOR_OFFSET_CM)
    float water_height_cm = TANK_HEIGHT_CM - (avg_distance_cm - SENSOR_OFFSET_CM);

    // Clamp to valid range (sensor noise can push reading slightly out of bounds)
    water_height_cm = constrain(water_height_cm, 0.0, TANK_HEIGHT_CM);

    // Convert height → percentage
    float percentage = (water_height_cm / TANK_HEIGHT_CM) * 100.0;

    Serial.println("[WaterLevel] Distance: " + String(avg_distance_cm) 
        + " cm → Level: " + String(percentage, 1) + "%");

    return percentage;
}