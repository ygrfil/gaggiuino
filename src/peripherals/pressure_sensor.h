/* 09:32 15/03/2023 - change triggering comment */
#ifndef PRESURE_SENSSOR_H
#define PRESURE_SENSSOR_H

#include <Arduino.h>

// Configuration for DMA-based readings
#define USE_DMA_FOR_PRESSURE_SENSOR 1  // DMA enabled with HAL driver access

// Configuration for Kalman filtering
#define USE_KALMAN_FILTER 1  // Enable Kalman filter for improved sensor fusion

// DMA buffer size - number of samples to collect
#define PRESSURE_DMA_BUFFER_SIZE 8

// Structure to hold pressure sensor DMA state
typedef struct {
  bool dmaActive;                             // Indicates if DMA transfer is active
  uint16_t rawReadings[PRESSURE_DMA_BUFFER_SIZE]; // Raw ADC values from DMA
  uint8_t readIndex;                          // Current read index in the buffer
  uint8_t writeIndex;                         // Current write index in the buffer
  float filteredPressure;                     // Latest filtered pressure value
} PressureDmaState_t;

#if USE_KALMAN_FILTER
// Kalman filter state structure for pressure sensor
typedef struct {
  float x;          // State estimate (pressure)
  float P;          // Estimation error covariance
  float Q;          // Process noise covariance
  float R;          // Measurement noise covariance
  float K;          // Kalman gain
  bool initialized; // Filter initialization flag
} KalmanState_t;
#endif

// Initialize I2C and ADS1X15 pressure sensor
void adsInit(void);

// Reset I2C bus if stuck
void i2cResetState(void);

// Get the latest pressure reading
float getPressure(void);

// Check for ADS errors and recover
bool getAdsError(void);

// Apply moving average filter to pressure readings
float movingAveragePressure(float newReading);

#if USE_KALMAN_FILTER
// Initialize Kalman filter for pressure sensor
void initKalmanFilter(void);

// Apply Kalman filter to pressure readings
float kalmanFilterPressure(float measurement);

// Reset Kalman filter state
void resetKalmanFilter(void);

// Tune Kalman filter parameters (for advanced users)
void tuneKalmanFilter(float processNoise, float measurementNoise);
#endif

#if USE_DMA_FOR_PRESSURE_SENSOR
// Initialize DMA for pressure sensor
void initDmaPressureReading(void);

// Start a DMA transfer for pressure readings
void startPressureDmaTransfer(void);

// DMA completion callback
void pressureDmaCallback(void);

// Process completed DMA readings
float processDmaPressureReadings(void);
#endif

#endif
