// include/sensors/flowSensor.h
#ifndef FLOW_SENSOR_H
#define FLOW_SENSOR_H

void flowSensor_init();

// Returns current flow rate in litres per minute
float readFlowRate();

// Returns total litres consumed since last reset
float getTotalLitres();

// Resets the daily litre counter (call at midnight)
void resetTotalLitres();

// ISR — called automatically by hardware on every pulse
// Must be in header so it can be registered with attachInterrupt()
// Do NOT call this directly in your code
void IRAM_ATTR flowPulseISR();

#endif