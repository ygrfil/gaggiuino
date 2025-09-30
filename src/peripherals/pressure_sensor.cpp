/* 09:32 15/03/2023 - change triggering comment */
#include "pressure_sensor.h"
#include "pindef.h"
#include "ADS1X15.h"
#include "../lcd/lcd.h"
#include "../log.h"
#include "i2c_bus_reset.h"

// Include STM32 HAL headers for DMA functionality
#if USE_DMA_FOR_PRESSURE_SENSOR
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_i2c.h"
#endif

#if defined SINGLE_BOARD
ADS1015 ADS(0x48);
#else
ADS1115 ADS(0x48);
#endif

float previousPressure;
float currentPressure;
const float MAX_PRESSURE_JUMP = 1.0f; // Maximum allowable pressure jump between readings
const float MAX_PRESSURE_VALUE = 12.0f; // Maximum allowable pressure value
const float MIN_PRESSURE_VALUE = 0.0f; // Minimum allowable pressure value
const int MAX_ERROR_COUNT = 5; // Maximum number of consecutive errors before resetting
int errorCount = 0;

// Moving average filter for pressure
const int PRESSURE_FILTER_SIZE = 8;
float pressureBuffer[PRESSURE_FILTER_SIZE];
int pressureBufferIndex = 0;
bool pressureBufferFilled = false;

// Median filter for outlier rejection
const int MEDIAN_FILTER_SIZE = 5;
float medianBuffer[MEDIAN_FILTER_SIZE];
int medianBufferIndex = 0;
bool medianBufferFilled = false;

// Pressure rate-of-change validation
float lastValidPressure = 0.0f;
unsigned long lastPressureTime = 0;
const float MAX_PRESSURE_RATE = 3.0f; // Maximum bar/second rate of change
const unsigned long MIN_TIME_BETWEEN_READINGS = 10; // Minimum milliseconds between readings

// Diagnostic counters
struct PressureDiagnostics {
  unsigned long totalReadings;
  unsigned long invalidReadings;
  unsigned long outlierRejections;
  unsigned long rateValidationRejections;
  unsigned long calibrationErrors;
  unsigned long lastDiagnosticReport;
  const unsigned long DIAGNOSTIC_REPORT_INTERVAL = 30000; // Report every 30 seconds
} pressureDiagnostics = {0, 0, 0, 0, 0, 0, 30000};

#if USE_KALMAN_FILTER
// Kalman filter state for pressure sensor
KalmanState_t pressureKalman;

// Kalman filter tuning parameters optimized for coffee machine pressure
const float KALMAN_PROCESS_NOISE = 0.01f;    // Q - How much the pressure changes (low for stable brewing)
const float KALMAN_MEASUREMENT_NOISE = 0.2f; // R - Sensor noise level (increased for better stability)
const float KALMAN_INITIAL_ERROR = 1.0f;     // Initial estimation error
#endif

#if USE_DMA_FOR_PRESSURE_SENSOR
// DMA state for pressure sensor
PressureDmaState_t pressureDmaState;

// DMA handle for I2C transfers
DMA_HandleTypeDef hdma_i2c_rx;

// I2C handle for communicating with ADS1X15
I2C_HandleTypeDef hi2c1;

// Buffer for I2C commands to ADS1X15
uint8_t i2cCmdBuffer[3];

// Flag to indicate whether DMA is initialized
bool dmaPressureInitialized = false;
#endif

void adsInit(void) {
  Wire.begin();
  Wire.setClock(100000); // 100kHz standard for ADS1X15
  delay(50); // Short, non-blocking startup delay
  
  LOG_INFO("Initializing ADS pressure sensor");
  
  // Reset pressure filter
  for (int i = 0; i < PRESSURE_FILTER_SIZE; i++) {
    pressureBuffer[i] = 0.0f;
  }
  pressureBufferIndex = 0;
  pressureBufferFilled = false;
  
  // Reset median filter
  for (int i = 0; i < MEDIAN_FILTER_SIZE; i++) {
    medianBuffer[i] = 0.0f;
  }
  medianBufferIndex = 0;
  medianBufferFilled = false;
  
  // Initialize rate-of-change tracking
  lastValidPressure = 0.0f;
  lastPressureTime = millis();
  
  // Reset diagnostics
  pressureDiagnostics.totalReadings = 0;
  pressureDiagnostics.invalidReadings = 0;
  pressureDiagnostics.outlierRejections = 0;
  pressureDiagnostics.rateValidationRejections = 0;
  pressureDiagnostics.calibrationErrors = 0;
  pressureDiagnostics.lastDiagnosticReport = millis();
  
  // Try to connect to the ADS chip
  bool connected = false;
  for (int attempts = 0; attempts < 5; attempts++) {
    if (ADS.begin()) {
      connected = true;
      break;
    }
    delay(100);
    i2cResetState();
    delay(100);
  }
  
  if (!connected) {
    LOG_ERROR("Failed to connect to ADS chip after multiple attempts");
  }
  
  ADS.setGain(0);      // 6.144V range
  ADS.setDataRate(4);  // Typical stable data rate
  ADS.setMode(0);      // continuous mode
  
  // Take several readings to fill the buffer and stabilize
  for (int i = 0; i < 3; i++) {
    (void)ADS.readADC(0);
    delay(5);
  }
  
  // Initialize pressure readings
  currentPressure = 0.0f;
  previousPressure = 0.0f;
  
  #if USE_KALMAN_FILTER
  // Initialize Kalman filter
  initKalmanFilter();
  #endif
  
  LOG_INFO("ADS pressure sensor initialized");
  
  #if USE_DMA_FOR_PRESSURE_SENSOR
  // Initialize DMA for pressure readings if enabled
  initDmaPressureReading();
  #endif
}

#if USE_DMA_FOR_PRESSURE_SENSOR
// Initialize DMA for I2C transfers to/from ADS1X15
void initDmaPressureReading(void) {
  // Reset DMA state
  memset(&pressureDmaState, 0, sizeof(PressureDmaState_t));
  
  LOG_INFO("Initializing DMA for pressure sensor");
  
  // Configure I2C1 handle for DMA operations
  // This assumes the Wire library uses I2C1 on this board
  hi2c1.Instance = I2C1;
  
  // Configure I2C - use same settings as Wire library
  hi2c1.Init.ClockSpeed = 50000;  // 50 kHz
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  
  // Configure DMA for I2C receive
  __HAL_RCC_DMA1_CLK_ENABLE();
  
  // Configure DMA stream for I2C1 Rx
  hdma_i2c_rx.Instance = DMA1_Stream0;
  hdma_i2c_rx.Init.Channel = DMA_CHANNEL_1;
  hdma_i2c_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_i2c_rx.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_i2c_rx.Init.MemInc = DMA_MINC_ENABLE;
  hdma_i2c_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_i2c_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_i2c_rx.Init.Mode = DMA_NORMAL;
  hdma_i2c_rx.Init.Priority = DMA_PRIORITY_HIGH;
  hdma_i2c_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  
  // Initialize DMA
  if (HAL_DMA_Init(&hdma_i2c_rx) != HAL_OK) {
    LOG_ERROR("DMA initialization failed for pressure sensor");
    return;
  }
  
  // Link DMA to I2C
  __HAL_LINKDMA(&hi2c1, hdmarx, hdma_i2c_rx);
  
  // Configure I2C command to read from ADS1X15 conversion register
  i2cCmdBuffer[0] = 0x00; // Conversion register address
  
  // Set up DMA interrupt
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  
  dmaPressureInitialized = true;
  LOG_INFO("DMA initialized for pressure sensor");
  
  // Start first DMA transfer
  startPressureDmaTransfer();
}

// Start a DMA transfer to read from ADS1X15
void startPressureDmaTransfer(void) {
  if (!dmaPressureInitialized) {
    return;
  }
  
  // Check if DMA is already active
  if (pressureDmaState.dmaActive) {
    return;
  }
  
  pressureDmaState.dmaActive = true;
  
  // Send command to ADS1X15 to read from conversion register - use hardcoded address 0x48
  HAL_I2C_Master_Transmit(&hi2c1, (0x48 << 1), i2cCmdBuffer, 1, 100);
  
  // Start DMA to receive data - use hardcoded address 0x48
  HAL_I2C_Master_Receive_DMA(&hi2c1, (0x48 << 1), 
                            (uint8_t*)&pressureDmaState.rawReadings[pressureDmaState.writeIndex], 
                            2); // 2 bytes for each reading (16-bit)
}

// DMA completion callback
void pressureDmaCallback(void) {
  // Update write index
  pressureDmaState.writeIndex = (pressureDmaState.writeIndex + 1) % PRESSURE_DMA_BUFFER_SIZE;
  
  // Mark DMA as inactive
  pressureDmaState.dmaActive = false;
  
  // Process the readings
  processDmaPressureReadings();
  
  // Start next transfer
  startPressureDmaTransfer();
}

// Process DMA readings and calculate filtered pressure
float processDmaPressureReadings(void) {
  // Check if we have any new readings
  if (pressureDmaState.readIndex == pressureDmaState.writeIndex) {
    return pressureDmaState.filteredPressure;
  }
  
  // Process all available readings
  float sum = 0.0f;
  int count = 0;
  
  while (pressureDmaState.readIndex != pressureDmaState.writeIndex) {
    // Convert raw reading to pressure with corrected calibration constants
    #if defined SINGLE_BOARD
    // ADS1015 12-bit: Fixed calibration constants for better accuracy
    float reading = (pressureDmaState.rawReadings[pressureDmaState.readIndex] - 200) / 120.0f; // 12bit corrected
    #else
    // ADS1115 16-bit: Fixed calibration constants for better accuracy  
    float reading = (pressureDmaState.rawReadings[pressureDmaState.readIndex] - 3200) / 1920.0f; // 16bit corrected
    #endif
    
    // Only include valid readings
    if (!isnan(reading) && reading >= MIN_PRESSURE_VALUE && reading <= MAX_PRESSURE_VALUE) {
      sum += reading;
      count++;
    }
    
    // Move to next reading
    pressureDmaState.readIndex = (pressureDmaState.readIndex + 1) % PRESSURE_DMA_BUFFER_SIZE;
  }
  
  // Calculate average if we have valid readings
  if (count > 0) {
    float avgPressure = sum / count;
    
    // Apply filtering
    if (abs(avgPressure - pressureDmaState.filteredPressure) <= MAX_PRESSURE_JUMP) {
      pressureDmaState.filteredPressure = 0.7f * avgPressure + 0.3f * pressureDmaState.filteredPressure;
    } else {
      // If jump is too large, blend more heavily with previous value
      pressureDmaState.filteredPressure = 0.9f * pressureDmaState.filteredPressure + 0.1f * avgPressure;
    }
  }
  
  return pressureDmaState.filteredPressure;
}

// DMA interrupt handler for I2C1 Rx
void DMA1_Stream0_IRQHandler(void) {
  HAL_DMA_IRQHandler(&hdma_i2c_rx);
  pressureDmaCallback();
}
#endif

// Calculate moving average of pressure readings
float movingAveragePressure(float newReading) {
  // Add new reading to buffer
  pressureBuffer[pressureBufferIndex] = newReading;
  pressureBufferIndex = (pressureBufferIndex + 1) % PRESSURE_FILTER_SIZE;
  
  if (pressureBufferIndex == 0) {
    pressureBufferFilled = true;
  }
  
  // Calculate average
  float sum = 0.0f;
  int count = pressureBufferFilled ? PRESSURE_FILTER_SIZE : pressureBufferIndex;
  
  for (int i = 0; i < count; i++) {
    sum += pressureBuffer[i];
  }
  
  return sum / count;
}

// Median filter to reject outliers
float medianFilterPressure(float newReading) {
  // Add new reading to median buffer
  medianBuffer[medianBufferIndex] = newReading;
  medianBufferIndex = (medianBufferIndex + 1) % MEDIAN_FILTER_SIZE;
  
  if (medianBufferIndex == 0) {
    medianBufferFilled = true;
  }
  
  // Create temporary array for sorting
  float tempBuffer[MEDIAN_FILTER_SIZE];
  int count = medianBufferFilled ? MEDIAN_FILTER_SIZE : medianBufferIndex;
  
  for (int i = 0; i < count; i++) {
    tempBuffer[i] = medianBuffer[i];
  }
  
  // Simple bubble sort for small array
  for (int i = 0; i < count - 1; i++) {
    for (int j = 0; j < count - i - 1; j++) {
      if (tempBuffer[j] > tempBuffer[j + 1]) {
        float temp = tempBuffer[j];
        tempBuffer[j] = tempBuffer[j + 1];
        tempBuffer[j + 1] = temp;
      }
    }
  }
  
  // Return median value
  return tempBuffer[count / 2];
}

// Validate pressure rate of change
bool validatePressureRate(float newPressure) {
  unsigned long currentTime = millis();
  
  // Check if enough time has passed since last reading
  if (currentTime - lastPressureTime < MIN_TIME_BETWEEN_READINGS) {
    return false; // Too soon for another reading
  }
  
  // Calculate rate of change
  float timeDelta = (currentTime - lastPressureTime) / 1000.0f; // Convert to seconds
  float pressureDelta = abs(newPressure - lastValidPressure);
  float rate = pressureDelta / timeDelta;
  
  // Check if rate is within acceptable limits
  if (rate > MAX_PRESSURE_RATE && lastPressureTime > 0) {
    pressureDiagnostics.rateValidationRejections++;
    LOG_INFO("Pressure rate too high: %.2f bar/s (delta: %.2f bar, time: %.3f s)", 
             (double)rate, (double)pressureDelta, (double)timeDelta);
    return false;
  }
  
  // Update tracking variables
  lastValidPressure = newPressure;
  lastPressureTime = currentTime;
  
  return true;
}

// Report pressure sensor diagnostics
void reportPressureDiagnostics() {
  unsigned long currentTime = millis();
  
  if (currentTime - pressureDiagnostics.lastDiagnosticReport > pressureDiagnostics.DIAGNOSTIC_REPORT_INTERVAL) {
    if (pressureDiagnostics.totalReadings > 0) {
      float invalidRate = (100.0f * pressureDiagnostics.invalidReadings) / pressureDiagnostics.totalReadings;
      float outlierRate = (100.0f * pressureDiagnostics.outlierRejections) / pressureDiagnostics.totalReadings;
      float rateRejectionRate = (100.0f * pressureDiagnostics.rateValidationRejections) / pressureDiagnostics.totalReadings;
      
      LOG_INFO("Pressure sensor diagnostics - Total: %lu, Invalid: %lu (%.1f%%), Outliers: %lu (%.1f%%), Rate rejects: %lu (%.1f%%), Cal errors: %lu",
               pressureDiagnostics.totalReadings,
               pressureDiagnostics.invalidReadings, (double)invalidRate,
               pressureDiagnostics.outlierRejections, (double)outlierRate, 
               pressureDiagnostics.rateValidationRejections, (double)rateRejectionRate,
               pressureDiagnostics.calibrationErrors);
    }
    
    pressureDiagnostics.lastDiagnosticReport = currentTime;
  }
}

#if USE_KALMAN_FILTER
// Initialize Kalman filter for pressure sensor
void initKalmanFilter(void) {
  pressureKalman.x = 0.0f;                      // Initial state estimate
  pressureKalman.P = KALMAN_INITIAL_ERROR;      // Initial estimation error covariance
  pressureKalman.Q = KALMAN_PROCESS_NOISE;      // Process noise covariance
  pressureKalman.R = KALMAN_MEASUREMENT_NOISE;  // Measurement noise covariance
  pressureKalman.K = 0.0f;                      // Initial Kalman gain
  pressureKalman.initialized = false;           // Not initialized until first measurement
  
  LOG_INFO("Kalman filter initialized for pressure sensor");
}

// Apply Kalman filter to pressure readings
float kalmanFilterPressure(float measurement) {
  // Handle initialization with first measurement
  if (!pressureKalman.initialized) {
    pressureKalman.x = measurement;
    pressureKalman.initialized = true;
    return measurement;
  }
  
  // Prediction step (simple model: pressure doesn't change much between readings)
  // x_pred = x (assuming constant pressure between readings)
  // P_pred = P + Q
  float x_pred = pressureKalman.x;
  float P_pred = pressureKalman.P + pressureKalman.Q;
  
  // Update step
  // Calculate Kalman gain: K = P_pred / (P_pred + R)
  pressureKalman.K = P_pred / (P_pred + pressureKalman.R);
  
  // Update state estimate: x = x_pred + K * (measurement - x_pred)
  pressureKalman.x = x_pred + pressureKalman.K * (measurement - x_pred);
  
  // Update estimation error covariance: P = (1 - K) * P_pred
  pressureKalman.P = (1.0f - pressureKalman.K) * P_pred;
  
  // Adaptive noise tuning based on measurement innovation
  float innovation = fabs(measurement - x_pred);
  if (innovation > 0.5f) {
    // Large innovation suggests increased measurement noise or rapid pressure change
    pressureKalman.R = min(KALMAN_MEASUREMENT_NOISE * 2.0f, 0.3f);
  } else {
    // Small innovation suggests stable conditions
    pressureKalman.R = max(KALMAN_MEASUREMENT_NOISE * 0.8f, 0.05f);
  }
  
  return pressureKalman.x;
}

// Reset Kalman filter state (useful when starting new shot)
void resetKalmanFilter(void) {
  pressureKalman.initialized = false;
  pressureKalman.P = KALMAN_INITIAL_ERROR;
  pressureKalman.R = KALMAN_MEASUREMENT_NOISE;
  LOG_INFO("Kalman filter reset");
}

// Tune Kalman filter parameters for different brewing scenarios
void tuneKalmanFilter(float processNoise, float measurementNoise) {
  // Validate parameters
  if (processNoise > 0.0f && processNoise < 1.0f) {
    pressureKalman.Q = processNoise;
  }
  if (measurementNoise > 0.0f && measurementNoise < 1.0f) {
    pressureKalman.R = measurementNoise;
  }
  LOG_INFO("Kalman filter tuned: Q=%.3f, R=%.3f", (double)pressureKalman.Q, (double)pressureKalman.R);
}
#endif

float getPressure(void) {  //returns sensor pressure data
  // Increment total readings counter for diagnostics
  pressureDiagnostics.totalReadings++;
  
  // Report diagnostics periodically
  reportPressureDiagnostics();
  
  #if USE_DMA_FOR_PRESSURE_SENSOR
  if (dmaPressureInitialized) {
    // Use DMA-based pressure reading
    return pressureDmaState.filteredPressure;
  }
  #endif
  
  // Check and reset I2C if needed
  if (getAdsError()) {
    errorCount++;
    pressureDiagnostics.calibrationErrors++;
    if (errorCount > MAX_ERROR_COUNT) {
      LOG_ERROR("Too many consecutive ADS errors (%d), reinitializing sensor", errorCount);
      adsInit();
      errorCount = 0;
    } else {
      LOG_INFO("ADS error detected, count: %d/%d", errorCount, MAX_ERROR_COUNT);
    }
    return previousPressure;
  }
  
  errorCount = 0;
  previousPressure = currentPressure;
  
  // Take multiple readings without blocking delays (ADS continuous mode)
  float sumReadings = 0.0f;
  int validReadings = 0;
  
  for (int i = 0; i < 4; i++) { // Take 4 readings for averaging
    float reading;
    #if defined SINGLE_BOARD
      // ADS1015 12-bit: Calibration tuned for typical 0-12 bar span
      // Adjust zero-offset and scale to your transducer if needed
      reading = (ADS.getValue() - 180) / 128.0f;
    #else
      // ADS1115 16-bit
      reading = (ADS.getValue() - 3000) / 2048.0f;
    #endif
    
    if (!isnan(reading) && reading >= MIN_PRESSURE_VALUE && reading <= MAX_PRESSURE_VALUE) {
      sumReadings += reading;
      validReadings++;
    } else {
      pressureDiagnostics.invalidReadings++;
      LOG_INFO("Invalid pressure reading: %.2f (NaN: %s, Range: %.2f-%.2f)", 
               (double)reading, isnan(reading) ? "yes" : "no", 
               (double)MIN_PRESSURE_VALUE, (double)MAX_PRESSURE_VALUE);
    }
    // No delay needed - ADS operates in continuous conversion mode
  }
  
  if (validReadings == 0) {
    LOG_ERROR("No valid pressure readings out of 4 attempts");
    pressureDiagnostics.invalidReadings++;
    return previousPressure;
  }
  
  float rawPressure = sumReadings / validReadings;
  
  if (abs(rawPressure - previousPressure) > MAX_PRESSURE_JUMP) {
    LOG_INFO("Pressure jump detected: %.2f to %.2f bar (delta: %.2f)", 
             (double)previousPressure, (double)rawPressure, 
             (double)(rawPressure - previousPressure));
    pressureDiagnostics.outlierRejections++;
    rawPressure = previousPressure * 0.9f + rawPressure * 0.1f; // More conservative blending
  }
  
  // Apply multi-stage filtering with median filter first to reject outliers
  #if USE_KALMAN_FILTER
  // Stage 1: Apply median filter to reject outliers
  float medianPressure = medianFilterPressure(rawPressure);
  
  // Stage 2: Validate rate of change
  if (!validatePressureRate(medianPressure)) {
    // Rate too high, use previous pressure with small adjustment toward new reading
    currentPressure = 0.95f * previousPressure + 0.05f * medianPressure;
    return currentPressure;
  }
  
  // Stage 3: Apply moving average to reduce high-frequency noise
  float movingAvgPressure = movingAveragePressure(medianPressure);
  
  // Stage 4: Apply Kalman filter for optimal sensor fusion
  float kalmanPressure = kalmanFilterPressure(movingAvgPressure);
  
  // Stage 5: Light exponential smoothing for final output stability
  currentPressure = 0.9f * kalmanPressure + 0.1f * previousPressure;
  #else
  // Traditional filtering with median filter (fallback)
  float medianPressure = medianFilterPressure(rawPressure);
  
  if (!validatePressureRate(medianPressure)) {
    currentPressure = 0.95f * previousPressure + 0.05f * medianPressure;
    return currentPressure;
  }
  
  float filteredPressure = movingAveragePressure(medianPressure);
  currentPressure = 0.85f * filteredPressure + 0.15f * previousPressure;
  #endif
  
  return currentPressure;
}

bool getAdsError(void) {
  // Reset the hw i2c to try and recover comms
  // on fail to do so throw error
  i2cResetState();

  // Throw error code on ADS malfunction/miswiring
  // Invalid Voltage error code: -100
  // Invalid gain error code: 255
  // Invalid mode error code: 254
  short result = ADS.getError();
  if (result == 0) return false; // No error
  
  char tmp[25];
  unsigned int check = snprintf(tmp, sizeof(tmp), "ADS error code: %i", result);
  if (check > 0 && check <= sizeof(tmp)) {
    lcdShowPopup(tmp);
    LOG_ERROR("ADS error: %d", result);
  }
  return true; // Error occurred
}

//Serial.print(digitalRead(PIN_SCL));    //should be HIGH
//Serial.println(digitalRead(PIN_SDA));   //should be HIGH, is LOW on stuck I2C bus
// ERROR CODE 1: I2C bus error. Could not clear sclPin clock line held low
// ERROR CODE 2: I2C bus error. Could not clear. sclPin clock line held low by slave clock for > 2sec
// ERROR CODE 3: I2C bus error. Could not clear. sdaPin data line held low
void i2cResetState(void) {
  if (digitalRead(PIN_WIRE_SDA) != HIGH || digitalRead(PIN_WIRE_SCL) != HIGH || !ADS.isConnected()) {
    LOG_INFO("Reset I2C pins");
    short result = I2C_ClearBus(PIN_WIRE_SDA, PIN_WIRE_SCL);
    char tmp[25];
    unsigned int check = snprintf(tmp, sizeof(tmp), "I2C error code: %i", result);
    if (check > 0 && check <= sizeof(tmp)) {
      if (result == 0) {
        // Bus cleared successfully, reinitialize ADS
        Wire.begin();
        delay(50);
        adsInit();
      } else {
        // Bus could not be cleared, show error
        lcdShowPopup(tmp);
        LOG_ERROR("I2C bus reset failed with code: %d", result);
      }
    }
    delay(50);
  }
}
