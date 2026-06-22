// src/sensors/phSensor.cpp
// ─────────────────────────────────────────────────────────────
// pH measurement using universal pH test strips + TCS34725
// RGB colour sensor (I2C on GPIO21 SDA, GPIO22 SCL)
//
// HOW IT WORKS:
// 1. Dip a pH test strip in the water sample for 5 seconds
// 2. Place the wet strip flat under the TCS34725 sensor face
//    (sensor face must be within 3mm of the strip surface)
// 3. Call readPH() — sensor reads RGB colour of the strip
// 4. RGB values are compared against a calibration lookup table
//    to return the estimated pH value
//
// CALIBRATION (done in lab):
// 1. Dip a strip in pH 4.0 buffer, place under sensor
//    Type RAWPH in Serial Monitor → record R,G,B values
//    Type CAL4 R G B in Serial Monitor
// 2. Repeat for pH 7.0 buffer with CAL7 R G B
// 3. Repeat for pH 10.0 buffer with CAL10 R G B
// 3-point calibration gives accurate readings across the range
//
// TCS34725 INTEGRATION:
// Connected via I2C: SDA=GPIO21, SCL=GPIO22
// These are free pins in our GPIO assignment document
// Onboard white LED provides consistent illumination
// ─────────────────────────────────────────────────────────────
#include "sensors/phSensor.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>

// TCS34725 with 700ms integration time for better colour accuracy
// Longer integration = more light gathered = more stable readings
static Adafruit_TCS34725 tcs = Adafruit_TCS34725(
    TCS34725_INTEGRATIONTIME_614MS,   // closest available to 700ms — longest integration time for best accuracy
    TCS34725_GAIN_1X
);

static bool sensorReady = false;

// ── Calibration lookup table ──────────────────────────────────
// Three calibration points: pH 4, 7, 10
// Each stores the R/G/B reading from a strip dipped in
// a known buffer solution at that pH
struct CalPoint {
    float pH;
    uint16_t r, g, b;
    bool set;
};

static CalPoint calTable[3] = {
    { 4.0,  0, 0, 0, false },
    { 7.0,  0, 0, 0, false },
    { 10.0, 0, 0, 0, false }
};

// Default calibration values — replace after lab calibration
// These are approximate values for a standard universal indicator strip
// under white LED illumination — real values will differ
static void loadDefaultCalibration() {
    // pH 4 — strip is typically orange/red
    calTable[0] = { 4.0,  180, 80,  60,  true };
    // pH 7 — strip is typically yellow/green
    calTable[1] = { 7.0,  120, 140, 80,  true };
    // pH 10 — strip is typically blue/purple
    calTable[2] = { 10.0, 60,  80,  160, true };
}

bool phSensor_init() {
    Wire.begin(); // I2C on default GPIO21 (SDA) and GPIO22 (SCL)

    if (!tcs.begin()) {
        Serial.println("[pH] ERROR: TCS34725 not found on I2C");
        Serial.println("[pH] Check wiring: SDA=GPIO21, SCL=GPIO22, VCC=3.3V");
        sensorReady = false;
        return false;
    }

    sensorReady = true;
    loadDefaultCalibration();
    Serial.println("[pH] TCS34725 colour sensor initialised on I2C (GPIO21/22)");
    Serial.println("[pH] Default calibration loaded — update with buffer solutions in lab");
    return true;
}

void readPH_rawColour(uint16_t &r, uint16_t &g, uint16_t &b, uint16_t &c) {
    if (!sensorReady) {
        r = g = b = c = 0;
        return;
    }
    tcs.getRawData(&r, &g, &b, &c);
    Serial.println("[pH] Raw colour — R:" + String(r)
        + " G:" + String(g)
        + " B:" + String(b)
        + " C:" + String(c));
}

float readPH() {
    if (!sensorReady) {
        Serial.println("[pH] ERROR: Sensor not initialised");
        return -1.0;
    }

    uint16_t r, g, b, c;
    tcs.getRawData(&r, &g, &b, &c);

    if (c == 0) {
        Serial.println("[pH] ERROR: No light detected — check LED and strip placement");
        return -1.0;
    }

    // Check all calibration points are set
    bool allSet = calTable[0].set && calTable[1].set && calTable[2].set;
    if (!allSet) {
        Serial.println("[pH] WARNING: Using default calibration — calibrate with buffer solutions");
    }

    // Find pH by interpolating between calibration points
    // Calculate colour distance from each calibration point
    // The closest calibration point determines the pH region
    // Linear interpolation between the two nearest points

    // Normalise RGB by clear channel to remove ambient light effects
    float nr = (c > 0) ? (float)r / c : 0;
    float ng = (c > 0) ? (float)g / c : 0;
    float nb = (c > 0) ? (float)b / c : 0;

    // Calculate Euclidean distance in normalised RGB space
    // from measured colour to each calibration point
    float bestDist1 = 999999, bestDist2 = 999999;
    int idx1 = 0, idx2 = 1;

    for (int i = 0; i < 3; i++) {
        if (!calTable[i].set) continue;
        float cn = calTable[i].r + calTable[i].g + calTable[i].b;
        if (cn == 0) continue;
        float cr = calTable[i].r / cn;
        float cg = calTable[i].g / cn;
        float cb = calTable[i].b / cn;

        float dist = sqrt(
            (nr - cr) * (nr - cr) +
            (ng - cg) * (ng - cg) +
            (nb - cb) * (nb - cb)
        );

        if (dist < bestDist1) {
            bestDist2 = bestDist1; idx2 = idx1;
            bestDist1 = dist;      idx1 = i;
        } else if (dist < bestDist2) {
            bestDist2 = dist; idx2 = i;
        }
    }

    // Interpolate pH between the two nearest calibration points
    float totalDist = bestDist1 + bestDist2;
    float pH;
    if (totalDist < 0.001) {
        // Exact match
        pH = calTable[idx1].pH;
    } else {
        // Weight inversely proportional to distance
        float w1 = bestDist2 / totalDist;
        float w2 = bestDist1 / totalDist;
        pH = calTable[idx1].pH * w1 + calTable[idx2].pH * w2;
    }

    // Clamp to valid range
    pH = constrain(pH, 0.0f, 14.0f);

    Serial.println("[pH] R:" + String(r) + " G:" + String(g)
        + " B:" + String(b) + " → pH: " + String(pH, 1));

    return pH;
}

void phSensor_calibrate(float known_pH, uint16_t r, uint16_t g, uint16_t b) {
    for (int i = 0; i < 3; i++) {
        if (abs(calTable[i].pH - known_pH) < 0.1) {
            calTable[i].r = r;
            calTable[i].g = g;
            calTable[i].b = b;
            calTable[i].set = true;
            Serial.println("[pH] Calibration set: pH " + String(known_pH, 1)
                + " → R:" + String(r)
                + " G:" + String(g)
                + " B:" + String(b));
            return;
        }
    }
    Serial.println("[pH] Unknown calibration point: " + String(known_pH, 1)
        + " (valid: 4.0, 7.0, 10.0)");
}