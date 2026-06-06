// src/sensors/leakSensor.cpp
// ─────────────────────────────────────────────────────────────
// FC-37 Resistive Moisture / Leak Detection Sensor
// Connected to GPIO32 (digital input)
//
// HOW IT WORKS:
// Two exposed metal contact pads on the sensor PCB.
// DRY:  no electrical path between pads → output HIGH
// WET:  water bridges the pads → circuit closes → output LOW
//
// IMPORTANT — Before using:
// Turn the onboard potentiometer on the FC-37 module until
// the onboard LED turns ON when you touch the pads with a
// wet finger. This sets the sensitivity threshold correctly.
// If the pot is not set, digital output may not trigger.
//
// DEBOUNCING:
// A single water droplet splashing can give a brief false
// reading. We require 3 consecutive wet readings with 20ms
// between them before reporting a leak. This prevents false
// alerts from splashing during normal tank fill operations.
//
// PLACEMENT:
// Place the sensor flat in the tray under the tank.
// It catches drips and overflow from the tank base.
// Also place one near any pipe joints if possible.
// ─────────────────────────────────────────────────────────────
#include "sensors/leakSensor.h"
#include "config.h"
#include <Arduino.h>

// Number of consecutive wet reads required before alerting
// Increase this value if you get false alerts during tank fill
static const int DEBOUNCE_READS = 3;

void leakSensor_init() {
    // FC-37 DO output is active-LOW — internal pull-up keeps
    // pin HIGH when dry, sensor pulls LOW when wet
    pinMode(PIN_LEAK_SENSOR, INPUT);
    Serial.println("[Leak] Sensor initialised on GPIO"
        + String(PIN_LEAK_SENSOR));
}

bool checkLeakSensor() {
    int wetCount = 0;

    for (int i = 0; i < DEBOUNCE_READS; i++) {
        // FC-37 DO: LOW = moisture detected, HIGH = dry
        if (digitalRead(PIN_LEAK_SENSOR) == LOW) {
            wetCount++;
        }
        delay(20); // short delay between reads for debounce
    }

    // Only report leak if ALL consecutive reads show wet
    bool leakDetected = (wetCount == DEBOUNCE_READS);

    if (leakDetected) {
        Serial.println("[Leak] ⚠ ALERT: Water/moisture detected!");
    } else {
        Serial.println("[Leak] Dry — no leak");
    }

    return leakDetected;
}