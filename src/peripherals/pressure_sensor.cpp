/* 09:32 15/03/2023 - change triggering comment */
#include "pressure_sensor.h"
#include "pindef.h"
#include "ADS1X15.h"
#include "../lcd/lcd.h"
#include "../log.h"
#include "i2c_bus_reset.h"

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

#if USE_KALMAN_FILTER
// Kalman filter state for pressure sensor
KalmanState_t pressureKalman;

// Kalman filter tuning parameters optimized for coffee machine pressure
const float KALMAN_PROCESS_NOISE = 0.01f;    // Q - How much the pressure changes (low for stable brewing)
const float KALMAN_MEASUREMENT_NOISE = 0.1f; // R - Sensor noise level (based on ADS1115 specs)
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
  Wire.setClock(50000); // Lower I2C clock to 50kHz for more stability
  delay(200); // Longer delay to give I2C bus time to stabilize
  
  LOG_INFO("Initializing ADS pressure sensor");
  
  // Reset pressure filter
  for (int i = 0; i < PRESSURE_FILTER_SIZE; i++) {
    pressureBuffer[i] = 0.0f;
  }
  pressureBufferIndex = 0;
  pressureBufferFilled = false;
  
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
  
  ADS.setGain(0);      // 6.144 volt
  ADS.setDataRate(2);  // Lower data rate for more stability (was 4)
  ADS.setMode(0);      // continuous mode
  
  // Take several readings to fill the buffer and stabilize
  for (int i = 0; i < 5; i++) {
    ADS.readADC(0);
    delay(20);
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
  
  // Use the I2C1 peripheral directly from STM32
  // This assumes the Wire library uses I2C1 on this board
  hi2c1 = I2C1_BASE;
  
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
    // Convert raw reading to pressure
    #if defined SINGLE_BOARD
    float reading = (pressureDmaState.rawReadings[pressureDmaState.readIndex] - 166) / 111.11f; // 12bit
    #else
    float reading = (pressureDmaState.rawReadings[pressureDmaState.readIndex] - 2666) / 1777.8f; // 16bit
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
  #if USE_DMA_FOR_PRESSURE_SENSOR
  if (dmaPressureInitialized) {
    // Use DMA-based pressure reading
    return pressureDmaState.filteredPressure;
  }
  #endif
  
  // Check and reset I2C if needed
  if (getAdsError()) {
    errorCount++;
    if (errorCount > MAX_ERROR_COUNT) {
      LOG_ERROR("Too many consecutive ADS errors, reinitializing");
      adsInit();
      errorCount = 0;
    }
    return previousPressure;
  }
  
  errorCount = 0;
  previousPressure = currentPressure;
  
  // Take multiple readings with reduced delay
  float sumReadings = 0.0f;
  int validReadings = 0;
  
  for (int i = 0; i < 4; i++) { // Increased from 3 to 4 readings
    float reading;
    #if defined SINGLE_BOARD
      reading = (ADS.getValue() - 166) / 111.11f; // 12bit
    #else
      reading = (ADS.getValue() - 2666) / 1777.8f; // 16bit
    #endif
    
    if (!isnan(reading) && reading >= MIN_PRESSURE_VALUE && reading <= MAX_PRESSURE_VALUE) {
      sumReadings += reading;
      validReadings++;
    }
    
    delay(2); // Reduced from 5ms to 2ms
  }
  
  if (validReadings == 0) {
    LOG_ERROR("No valid pressure readings");
    return previousPressure;
  }
  
  float rawPressure = sumReadings / validReadings;
  
  if (abs(rawPressure - previousPressure) > MAX_PRESSURE_JUMP) {
    LOG_ERROR("Pressure jump too large: %f to %f", (double)previousPressure, (double)rawPressure);
    rawPressure = previousPressure * 0.9f + rawPressure * 0.1f; // More conservative blending
  }
  
  // Apply filtering - choose between Kalman and traditional filtering
  #if USE_KALMAN_FILTER
  // First apply moving average to reduce high-frequency noise
  float movingAvgPressure = movingAveragePressure(rawPressure);
  
  // Then apply Kalman filter for optimal sensor fusion
  float kalmanPressure = kalmanFilterPressure(movingAvgPressure);
  
  // Light exponential smoothing for final output stability
  currentPressure = 0.9f * kalmanPressure + 0.1f * previousPressure;
  #else
  // Traditional filtering (fallback)
  float filteredPressure = movingAveragePressure(rawPressure);
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
