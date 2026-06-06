// include/sensors/leakSensor.h
#ifndef LEAK_SENSOR_H
#define LEAK_SENSOR_H

void leakSensor_init();

// Returns true if moisture/water is detected at sensor pads
bool checkLeakSensor();

#endif