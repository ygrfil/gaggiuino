/* 09:32 15/03/2023 - change triggering comment */
/* SIMPLIFIED: Removed 5+ layers of filtering - main loop's Kalman filter handles smoothing */
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

// Simple state - no complex filter buffers needed
static float previousPressure = 0.0f;
static int errorCount = 0;

// Constants
static const float MAX_PRESSURE_JUMP = 1.5f;  // Max allowable pressure change per reading
static const float MAX_PRESSURE_VALUE = 12.0f;
static const float MIN_PRESSURE_VALUE = -0.5f;  // Allow small negative for calibration offset
static const int MAX_ERROR_COUNT = 5;

void adsInit(void) {
  Wire.begin();
  Wire.setClock(100000);  // 100kHz for ADS1X15
  delay(50);
  
  LOG_INFO("Initializing ADS pressure sensor");
  
  // Try to connect
  bool connected = false;
  for (int attempts = 0; attempts < 3; attempts++) {
    if (ADS.begin()) {
      connected = true;
      break;
    }
    delay(50);
    i2cResetState();
  }
  
  if (!connected) {
    LOG_ERROR("Failed to connect to ADS chip");
  }
  
  ADS.setGain(0);      // 6.144V range
  ADS.setDataRate(4);  // Standard data rate
  ADS.setMode(0);      // Continuous mode
  
  // Take initial reading
  (void)ADS.readADC(0);
  delay(10);
  
  previousPressure = 0.0f;
  errorCount = 0;
  
  LOG_INFO("ADS pressure sensor initialized");
}

float getPressure(void) {
  // Check for errors
  if (getAdsError()) {
    errorCount++;
    if (errorCount > MAX_ERROR_COUNT) {
      LOG_ERROR("Too many ADS errors, reinitializing");
      adsInit();
      errorCount = 0;
    }
    return previousPressure;
  }
  errorCount = 0;
  
  // Read sensor
  float reading;
  #if defined SINGLE_BOARD
    // ADS1015 12-bit calibration
    reading = (ADS.getValue() - 180) / 128.0f;
  #else
    // ADS1115 16-bit calibration
    reading = (ADS.getValue() - 3000) / 2048.0f;
  #endif
  
  // Validity check
  if (isnan(reading) || reading < MIN_PRESSURE_VALUE || reading > MAX_PRESSURE_VALUE) {
    return previousPressure;
  }
  
  // Simple jump protection - blend if change is too large
  if (fabsf(reading - previousPressure) > MAX_PRESSURE_JUMP) {
    reading = previousPressure * 0.7f + reading * 0.3f;
  }
  
  previousPressure = reading;
  return reading;
}

bool getAdsError(void) {
  i2cResetState();
  short result = ADS.getError();
  if (result == 0) return false;
  
  char tmp[25];
  snprintf(tmp, sizeof(tmp), "ADS error: %i", result);
  lcdShowPopup(tmp);
  LOG_ERROR("ADS error: %d", result);
  return true;
}

void i2cResetState(void) {
  if (digitalRead(PIN_WIRE_SDA) != HIGH || digitalRead(PIN_WIRE_SCL) != HIGH || !ADS.isConnected()) {
    LOG_INFO("Reset I2C pins");
    short result = I2C_ClearBus(PIN_WIRE_SDA, PIN_WIRE_SCL);
    if (result == 0) {
      Wire.begin();
      delay(20);
    } else {
      LOG_ERROR("I2C bus reset failed: %d", result);
    }
  }
}
