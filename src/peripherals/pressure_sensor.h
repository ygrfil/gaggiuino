/* 09:32 15/03/2023 - change triggering comment */
#ifndef PRESURE_SENSSOR_H
#define PRESURE_SENSSOR_H

#include <Arduino.h>

// Initialize I2C and ADS1X15 pressure sensor
void adsInit(void);

// Reset I2C bus if stuck
void i2cResetState(void);

// Get the latest pressure reading (simple, main loop does Kalman smoothing)
float getPressure(void);

// Check for ADS errors and recover
bool getAdsError(void);

#endif
