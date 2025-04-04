/* 09:32 15/03/2023 - change triggering comment */
#ifndef THERMOCOUPLE_H
#define THERMOCOUPLE_H

#include "pindef.h"

#if defined SINGLE_BOARD
#include <Adafruit_MAX31855.h>
// Create the SPI interface for the thermocouple with explicit clock, MISO and MOSI pins
SPIClass thermoSPI(thermoDI, thermoDO, thermoCLK);
// Create the thermocouple instance with chip select and SPI interface
Adafruit_MAX31855 thermocouple(thermoCS, &thermoSPI);
#else
#include <max6675.h>
// Create the SPI interface for the thermocouple with explicit clock, MISO and MOSI pins
SPIClass thermoSPI(thermoDI, thermoDO, thermoCLK);
// Create the thermocouple instance with chip select and SPI interface
MAX6675 thermocouple(thermoCS, &thermoSPI);
#endif

static inline void thermocoupleInit(void) {
  // Configure SPI pins
  pinMode(thermoCLK, OUTPUT);
  pinMode(thermoDO, INPUT);
  pinMode(thermoCS, OUTPUT);
  digitalWrite(thermoCS, HIGH); // Ensure CS is high initially
  
  // Begin SPI communication
  thermoSPI.begin();
  thermoSPI.setClockDivider(SPI_CLOCK_DIV8); // Lower SPI clock for reliability
  thermoSPI.setBitOrder(MSBFIRST);
  thermoSPI.setDataMode(SPI_MODE0);
  
  // Initialize thermocouple
  thermocouple.begin();
  
  // Small delay to ensure initialization
  delay(100);
}

static inline float thermocoupleRead(void) {
  // Read temperature and check for errors
  float reading = thermocouple.readCelsius();
  
  // Check for valid reading (MAX31855 returns NAN on errors)
  if (isnan(reading) || reading < 0 || reading > 300) {
    // Retry once on error
    delay(10);
    reading = thermocouple.readCelsius();
    
    // If still invalid, return a default "room temperature" to prevent system freeze
    if (isnan(reading) || reading < 0 || reading > 300) {
      return 35.0; // Return reasonable default to prevent system halt
    }
  }
  
  return reading;
}

#endif
