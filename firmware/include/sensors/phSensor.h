// include/sensors/phSensor.h
// pH measurement using pH test strips + TCS34725 RGB colour sensor
// Place a dipped pH strip under the sensor for a reading
#ifndef PH_SENSOR_H
#define PH_SENSOR_H

#include <stdint.h> 

// Call once in setup()
// Returns false if TCS34725 not detected on I2C
bool phSensor_init();

// Returns estimated pH value (0.0 to 14.0)
// Returns -1.0 if sensor not ready or reading failed
// Place a freshly dipped pH strip under sensor before calling
float readPH();

// Returns raw R, G, B, C values from colour sensor (for calibration)
// Useful in Serial Monitor: type RAWPH to trigger this
void readPH_rawColour(uint16_t &r, uint16_t &g, uint16_t &b, uint16_t &c);

// Calibration — build lookup table from known buffer solutions
// Call with a strip dipped in known pH buffer and the measured RGB
void phSensor_calibrate(float known_pH, uint16_t r, uint16_t g, uint16_t b);

#endif