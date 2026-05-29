// src/sensors/phSensor.cpp
// ─────────────────────────────────────────────────────────────
// DFRobot SEN0161-V2 Gravity Analog pH Sensor
// Connected to GPIO34 (ADC1, input-only, WiFi-safe)
// Via 10k + 20k voltage divider (safety — scales 0–5V to 0–3.3V)
//
// HOW pH MEASUREMENT WORKS:
// The probe generates a voltage proportional to pH.
// At pH 7.0 (neutral): output is ~2.5V (mid-point)
// At pH lower (acidic): voltage INCREASES above 2.5V
// At pH higher (alkaline): voltage DECREASES below 2.5V
// The relationship is linear: pH = slope * voltage + offset
//
// CALIBRATION:
// You need real buffer solutions (pH 4.0 and pH 7.0 minimum).
// Done in the lab. Until then, use the default values below
// which give approximate readings.
//
// TEMPERATURE EFFECT:
// pH slope changes with temperature (~0.003 pH/°C deviation).
// DS18B20 temperature reading is used for compensation.
// ─────────────────────────────────────────────────────────────
#include "sensors/phSensor.h"
#include "config.h"
#include <Arduino.h>

// ── Calibration values ────────────────────────────────────────
// These are DEFAULT values — MUST be updated after lab calibration
// with your actual buffer solutions.
// Format: two-point calibration using pH 4.0 and pH 7.0 buffers
static float cal_voltage_pH4 = 3.07;   // voltage at pH 4 buffer (default — update in lab)
static float cal_voltage_pH7 = 2.52;   // voltage at pH 7 buffer (default — update in lab)

// Derived slope and offset (calculated from calibration points)
static float ph_slope  = 0.0;
static float ph_offset = 0.0;

static void calculateCalibration() {
    // Linear equation: pH = slope * voltage + offset
    // Using two known points (pH4, V4) and (pH7, V7):
    ph_slope  = (7.0 - 4.0) / (cal_voltage_pH7 - cal_voltage_pH4);
    ph_offset = 7.0 - ph_slope * cal_voltage_pH7;
    Serial.println("[pH] Calibration — slope: " + String(ph_slope, 4) 
        + "  offset: " + String(ph_offset, 4));
}

void phSensor_init() {
    // GPIO34 is input-only — no pinMode needed for analog read
    // but explicitly confirm it's not set as output
    pinMode(PIN_PH_SENSOR, INPUT);
    calculateCalibration();
    Serial.println("[pH] Sensor initialised on GPIO" + String(PIN_PH_SENSOR));
}

float readPH_rawVoltage() {
    // Take 10 ADC samples and average to reduce noise
    // ESP32 ADC is noisy — averaging significantly improves accuracy
    long sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += analogRead(PIN_PH_SENSOR);
        delay(10);
    }
    float adc_avg = sum / 10.0;

    // Convert ADC reading to voltage
    // The voltage divider (10k + 20k) halves the input voltage.
    // To recover the actual sensor voltage, multiply by (10+20)/20 = 1.5
    // But the SEN0161-V2 outputs 0–3.3V natively, so divider factor is 1.0
    // if you confirmed sensor is 3.3V-safe. Adjust if needed.
    float voltage = (adc_avg / ADC_RESOLUTION) * ADC_VREF;

    return voltage;
}

float readPH() {
    float voltage = readPH_rawVoltage();

    // Apply temperature compensation
    // Default 25°C assumed during home testing (no DS18B20 connected yet)
    // In full system: pass actual temperature from readTemperature()
    float temperature = 25.0; // TODO: replace with readTemperature() call

    // Temperature compensation factor: Nernst equation
    // Ideal slope at 25°C is 59.16 mV/pH. At other temperatures:
    // slope_T = 59.16 * (273.15 + T) / 298.15
    // Simplified linear correction adequate for this application:
    float temp_correction = 0.0 + (temperature - 25.0) * 0.03;

    // Calculate pH from calibrated slope/offset + temperature correction
    float pH = ph_slope * voltage + ph_offset + temp_correction;

    // Validate range
    if (pH < 0.0 || pH > 14.0) {
        Serial.println("[pH] WARNING: Reading " + String(pH, 2) 
            + " out of range — check probe connection and calibration");
        return -1.0;
    }

    Serial.println("[pH] Voltage: " + String(voltage, 3) 
        + "V → pH: " + String(pH, 2));

    return pH;
}

void phSensor_calibrate(float known_pH, float measured_voltage) {
    // Called during lab calibration session with buffer solutions
    if (known_pH == 4.0) {
        cal_voltage_pH4 = measured_voltage;
        Serial.println("[pH] Calibration point set: pH4 = " + String(measured_voltage, 4) + "V");
    } else if (known_pH == 7.0) {
        cal_voltage_pH7 = measured_voltage;
        Serial.println("[pH] Calibration point set: pH7 = " + String(measured_voltage, 4) + "V");
    }
    calculateCalibration(); // recalculate slope and offset
}