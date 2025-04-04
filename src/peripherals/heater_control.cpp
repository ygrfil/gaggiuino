/* Heater Control Implementation */
#include "heater_control.h"
#include "pindef.h"
#include "hw_timer.h"
#include <Arduino.h>
#include "../log.h"

// Implementation of the heater control functions that are forward-declared in heater_control.h

// Actuating the heater element - ON
void setBoilerOn(void) {
  #if defined(USE_HARDWARE_TIMER_PWM)
    LOG_INFO("Boiler: ON (PWM)");
    heaterHardwareOn();
  #else
    LOG_INFO("Boiler: ON (Direct)");
    digitalWrite(relayPin, HIGH);  // boilerPin -> HIGH
  #endif
}

// Actuating the heater element - OFF
void setBoilerOff(void) {
  #if defined(USE_HARDWARE_TIMER_PWM)
    LOG_INFO("Boiler: OFF (PWM)");
    heaterHardwareOff();
  #else
    LOG_INFO("Boiler: OFF (Direct)");
    digitalWrite(relayPin, LOW);  // boilerPin -> LOW
  #endif
}

// We don't implement setPumpOff here - it's already implemented in pump.cpp
// The forward declaration in heater_control.h is just to break circular dependencies 