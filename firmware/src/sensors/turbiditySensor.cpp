// src/sensors/turbiditySensor.cpp
// ─────────────────────────────────────────────────────────────
// DIY Turbidity Sensor — LDR + Bright White LED
// Connected to GPIO35 (ADC1, input-only, WiFi-safe)
// LED powered from 3.3V GPIO or directly from 3.3V rail
//
// HOW IT WORKS:
// LED shines light through the water sample.
// LDR on the opposite side measures how much reaches it.
// CLEAN water → most light passes → HIGH LDR voltage reading
// DIRTY water → light scattered/blocked → LOWER voltage reading
// Voltage is INVERSELY proportional to turbidity.
//
// PHYSICAL SETUP:
// LED and LDR face each other through the water.
// Gap between them should be 2–3 cm through water.
// Keep consistent geometry for repeatable results.
//
// CALIBRATION (done in lab):
// 1. Fill container with distilled/clean water
// 2. Call readTurbidity_rawVoltage() → record as cal_voltage_0NTU
// 3. Prepare a muddy/turbid water sample
// 4. Call readTurbidity_rawVoltage() → record as cal_voltage_dirty
// 5. Update the calibration constants below
// ─────────────────────────────────────────────────────────────
#include "sensors/turbiditySensor.h"
#include "config.h"
#include <Arduino.h>

// ── Calibration constants ─────────────────────────────────────
// Update these after lab calibration session
// Default values are approximate — readings will be rough until calibrated
static float cal_voltage_clean = 3.10;  // voltage in clean distilled water (≈0 NTU)
static float cal_voltage_dirty = 1.80;  // voltage in turbid reference sample
static float cal_NTU_dirty     = 500.0; // NTU value of your turbid reference

void turbiditySensor_init() {
    // GPIO35 is input-only — no pinMode needed for analog
    // but set explicitly for clarity
    pinMode(PIN_TURBIDITY, INPUT);
    Serial.println("[Turbidity] Sensor initialised on GPIO"
        + String(PIN_TURBIDITY));
}

float readTurbidity_rawVoltage() {
    // Average 10 ADC samples to reduce noise
    // ESP32 ADC is noisy — averaging significantly improves stability
    long sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += analogRead(PIN_TURBIDITY);
        delay(10);
    }
    float adc_avg = sum / 10.0;
    float voltage = (adc_avg / ADC_RESOLUTION) * ADC_VREF;
    return voltage;
}

float readTurbidity() {
    float voltage = readTurbidity_rawVoltage();

    // Basic sanity check on voltage range
    if (voltage < 0.2 || voltage > 3.4) {
        Serial.println("[Turbidity] ERROR: Voltage " + String(voltage, 3)
            + "V out of expected range — check LDR and LED connection");
        return -1.0;
    }

    // Convert voltage to NTU using linear calibration
    // Higher voltage = cleaner water = lower NTU
    // Formula: NTU = ((clean_V - measured_V) / (clean_V - dirty_V)) * dirty_NTU
    float NTU = ((cal_voltage_clean - voltage) /
                 (cal_voltage_clean - cal_voltage_dirty)) * cal_NTU_dirty;

    // Clamp to 0 minimum
    // Small negative values are possible near 0 NTU due to sensor noise
    NTU = max(0.0f, NTU);

    Serial.println("[Turbidity] Voltage: " + String(voltage, 3)
        + "V → " + String(NTU, 1) + " NTU");

    return NTU;
}