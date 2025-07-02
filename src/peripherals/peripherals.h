/* 09:32 15/03/2023 - change triggering comment */
#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include "pindef.h"
#include "hw_timer.h"
#include <Arduino.h>
#include "../log.h"

// Flag to enable hardware timer-based PWM
// Using TIM3 for PWM (optimal choice when no HX711 scales present)
// DISABLED - PWM causing temperature oscillations, needs more investigation
// #define USE_HARDWARE_TIMER_PWM

static inline void pinInit(void) {
  #if defined(LEGO_VALVE_RELAY)
    pinMode(valvePin, OUTPUT_OPEN_DRAIN);
  #else
    pinMode(valvePin, OUTPUT);
  #endif

  #if defined(USE_HARDWARE_TIMER_PWM)
    // For hardware PWM, the pin will be configured by the timer
    // Just initialize it as output for safety
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW);
    // Initialize hardware timer for heater control
    // If initialization fails, we'll use digital IO instead
    if (!heaterTimerInit()) {
      // Fallback to digital IO mode if timer initialization fails
      #ifdef HARDWARE_PWM_REQUIRED
        // If hardware PWM is required but not available, log an error
        LOG_ERROR("Hardware PWM init failed for relay pin. Check pin compatibility.");
      #endif
    }
  #else
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW); // Ensure heater starts off
    LOG_INFO("Using direct digital control for heater element");
  #endif

  #ifdef steamValveRelayPin
  pinMode(steamValveRelayPin, OUTPUT);
  #endif
  #ifdef steamBoilerRelayPin
  pinMode(steamBoilerRelayPin, OUTPUT);
  #endif
  pinMode(brewPin,  INPUT_PULLUP);
  pinMode(steamPin, INPUT_PULLUP);
  #ifdef waterPin
  pinMode(waterPin, INPUT_PULLUP);
  #endif
}

// Actuating the heater element
static inline void setBoilerOn(void) {
  #if defined(USE_HARDWARE_TIMER_PWM)
    heaterHardwareOn();
  #else
    digitalWrite(relayPin, HIGH);  // boilerPin -> HIGH
  #endif
}

static inline void setBoilerOff(void) {
  #if defined(USE_HARDWARE_TIMER_PWM)
    heaterHardwareOff();
  #else
    digitalWrite(relayPin, LOW);  // boilerPin -> LOW
  #endif
}

// Set heater PWM duty cycle (0-100%)
static inline void setBoilerPWM(uint8_t dutyCycle) {
  #if defined(USE_HARDWARE_TIMER_PWM)
    setHeaterDutyCycle(dutyCycle);
  #else
    // No software PWM implementation - just on/off
    if (dutyCycle > 50) {
      setBoilerOn();
    } else {
      setBoilerOff();
    }
  #endif
}

static inline void setSteamValveRelayOn(void) {
  #ifdef steamValveRelayPin
  digitalWrite(steamValveRelayPin, HIGH);  // steamValveRelayPin -> HIGH
  #endif
}

static inline void setSteamValveRelayOff(void) {
  #ifdef steamValveRelayPin
  digitalWrite(steamValveRelayPin, LOW);  // steamValveRelayPin -> LOW
  #endif
}

static inline void setSteamBoilerRelayOn(void) {
  #ifdef steamBoilerRelayPin
  digitalWrite(steamBoilerRelayPin, HIGH);  // steamBoilerRelayPin -> HIGH
  #endif
}

static inline void setSteamBoilerRelayOff(void) {
  #ifdef steamBoilerRelayPin
  digitalWrite(steamBoilerRelayPin, LOW);  // steamBoilerRelayPin -> LOW
  #endif
}

//Function to get the state of the brew switch button
//returns true or false based on the read P(power) value
static inline bool brewState(void) {
  return digitalRead(brewPin) == LOW; // pin will be low when switch is ON.
}

// Returns HIGH when switch is OFF and LOW when ON
// pin will be high when switch is ON.
static inline bool steamState(void) {
  return digitalRead(steamPin) == LOW; // pin will be low when switch is ON.
}

static inline bool waterPinState(void) {
  #ifdef waterPin
  return digitalRead(waterPin) == LOW; // pin will be low when switch is ON.
  #else
  return false;
  #endif
}

static inline void openValve(void) {
  #if defined LEGO_VALVE_RELAY
    digitalWrite(valvePin, LOW);
  #else
    digitalWrite(valvePin, HIGH);
  #endif
}

static inline void closeValve(void) {
  #if defined LEGO_VALVE_RELAY
    digitalWrite(valvePin, HIGH);
  #else
    digitalWrite(valvePin, LOW);
  #endif
}

#endif
