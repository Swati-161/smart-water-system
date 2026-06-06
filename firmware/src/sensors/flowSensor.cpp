// src/sensors/flowSensor.cpp
// ─────────────────────────────────────────────────────────────
// YF-S201 Hall-Effect Water Flow Meter
// Connected to GPIO27 (digital interrupt pin)
//
// HOW IT WORKS:
// A small paddle wheel inside the sensor spins as water flows.
// A magnet on the wheel triggers a Hall-effect sensor each rotation.
// Each rotation generates one digital pulse on the signal wire.
// Counting pulses per second gives us flow rate.
//
// CALIBRATION FACTOR:
// YF-S201 spec: 7.5 pulses per second = 1 litre per minute
// So: flow_LPM = (pulses_per_second / 7.5)
// If your readings seem off by a fixed factor after lab testing,
// adjust the 7.5 value in readFlowRate() accordingly.
//
// WHY INTERRUPT DRIVEN (not polling):
// At 10 L/min the sensor outputs 75 pulses/second.
// At 30 L/min it outputs 225 pulses/second.
// If you used digitalRead() in loop() you would miss pulses
// any time the ESP32 is doing WiFi, MQTT, or sensor reads.
// Hardware interrupt fires INSTANTLY on every pulse regardless
// of what else is running — no pulses are ever missed.
//
// IRAM_ATTR: Places the ISR function in fast internal RAM
// so it executes quickly without loading from flash.
// ─────────────────────────────────────────────────────────────
#include "sensors/flowSensor.h"
#include "config.h"
#include <Arduino.h>

// volatile tells the compiler this variable can change
// at any time (inside an interrupt) — prevents the compiler
// from caching it in a register and missing updates
volatile unsigned long pulseCount    = 0;

static unsigned long lastReadTime     = 0;
static unsigned long lastPulseSnapshot = 0;
static float totalLitres             = 0.0;

// ── ISR — Interrupt Service Routine ──────────────────────────
// Called AUTOMATICALLY by hardware on every RISING pulse edge.
// Keep it as SHORT as possible — just increment the counter.
// No Serial.print, no delay(), no complex logic inside ISR.
void IRAM_ATTR flowPulseISR() {
    pulseCount++;
}

void flowSensor_init() {
    pinMode(PIN_FLOW_METER, INPUT_PULLUP);

    // Register the ISR on RISING edge (LOW → HIGH transition)
    attachInterrupt(digitalPinToInterrupt(PIN_FLOW_METER),
                    flowPulseISR, RISING);

    lastReadTime = millis();
    Serial.println("[Flow] Sensor initialised on GPIO"
        + String(PIN_FLOW_METER) + " with hardware interrupt");
}

float readFlowRate() {
    unsigned long now        = millis();
    unsigned long elapsed_ms = now - lastReadTime;

    if (elapsed_ms == 0) return 0.0;

    // Safely snapshot pulse count
    // noInterrupts() / interrupts() prevents the ISR from
    // incrementing pulseCount mid-read causing a corrupt value
    noInterrupts();
    unsigned long currentPulses = pulseCount;
    interrupts();

    unsigned long newPulses = currentPulses - lastPulseSnapshot;
    lastPulseSnapshot       = currentPulses;
    lastReadTime            = now;

    // Convert pulses → flow rate
    // newPulses occurred over elapsed_ms milliseconds
    float pulses_per_second = newPulses / (elapsed_ms / 1000.0);
    float flow_LPM          = pulses_per_second / 7.5;

    // Accumulate total volume
    // Volume (L) = flow_rate (L/min) × time_elapsed (min)
    float elapsed_min = elapsed_ms / 60000.0;
    totalLitres      += flow_LPM * elapsed_min;

    Serial.println("[Flow] " + String(newPulses) + " pulses in "
        + String(elapsed_ms) + "ms → "
        + String(flow_LPM, 2) + " L/min  |  Total: "
        + String(totalLitres, 2) + " L");

    return flow_LPM;
}

float getTotalLitres() {
    return totalLitres;
}

void resetTotalLitres() {
    totalLitres = 0.0;
    Serial.println("[Flow] Daily total reset to 0.0 L");
}