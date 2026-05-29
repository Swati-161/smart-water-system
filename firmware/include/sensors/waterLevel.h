// include/sensors/waterLevel.h
#ifndef WATER_LEVEL_H
#define WATER_LEVEL_H

// Call once in setup()
void waterLevel_init();

// Returns water level as percentage (0.0 to 100.0)
// Returns -1.0 if reading failed or out of range
float readWaterLevel();

// Returns raw distance in cm from sensor to water surface
float readWaterLevelRaw_cm();

#endif