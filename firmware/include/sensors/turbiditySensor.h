// include/sensors/turbiditySensor.h
#ifndef TURBIDITY_SENSOR_H
#define TURBIDITY_SENSOR_H

void turbiditySensor_init();

// Returns turbidity in NTU (0 = clear, higher = dirtier)
// Returns -1.0 if reading failed
float readTurbidity();

// Returns raw voltage at sensor pin (for calibration)
float readTurbidity_rawVoltage();

#endif