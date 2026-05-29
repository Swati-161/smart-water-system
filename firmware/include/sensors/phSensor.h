// include/sensors/phSensor.h
#ifndef PH_SENSOR_H
#define PH_SENSOR_H

// Call once in setup() — loads calibration values
void phSensor_init();

// Returns calibrated pH value (0.0 to 14.0)
// Returns -1.0 if reading failed
float readPH();

// Returns raw ADC voltage at pH pin (for calibration use)
float readPH_rawVoltage();

// Calibration — call during lab calibration session
// Pass known pH of buffer solution and measured raw voltage
void phSensor_calibrate(float known_pH, float measured_voltage);

#endif