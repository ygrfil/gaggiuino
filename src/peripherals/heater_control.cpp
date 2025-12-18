/* Simple Heater Control Implementation - Restored Original Functionality */
#include "heater_control.h"
#include "pindef.h"
#include <Arduino.h>
#include "../log.h"
#include "../../lib/Common/system_state.h"

extern SystemState systemState;

// Simple heater control - just like original firmware

// Actuating the heater element - ON
void setBoilerOn(void) {
  if (systemState.shutdownActive) {
    // Block heater during standby
    digitalWrite(relayPin, LOW);
    return;
  }
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);  // boilerPin -> HIGH
}

// Actuating the heater element - OFF
void setBoilerOff(void) {
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);  // boilerPin -> LOW
}

// Stub functions to maintain compatibility - do nothing
void initPIDTemperatureControl() {
    // Do nothing - using simple temperature control
}

void setPIDHeaterOutput(float dutyCycle) {
    // Do nothing - not using PID
    (void)dutyCycle;
}

float computePIDTemperatureControl(float setpoint, float currentTemp, bool isBrewMode) {
    // Do nothing - not using PID
    (void)setpoint;
    (void)currentTemp;
    (void)isBrewMode;
    return 0.0f;
}

void resetPIDController() {
    // Do nothing - not using PID
}

void setPIDTunings(float kp, float ki, float kd) {
    // Do nothing - not using PID
    (void)kp;
    (void)ki;
    (void)kd;
}

void enablePIDDebugMode(bool enabled) {
    // Do nothing - not using PID
    (void)enabled;
}

float getPIDOutput() {
    return 0.0f;
}

float getPIDError() {
    return 0.0f;
}

void getPIDTunings(float &kp, float &ki, float &kd) {
    kp = ki = kd = 0.0f;
}

void enablePIDDiagnosticMode(bool enabled) {
    // Do nothing - not using PID
    (void)enabled;
}

void logPIDDiagnostics(float setpoint, float currentTemp, float output, bool heaterState) {
    // Do nothing - not using PID
    (void)setpoint;
    (void)currentTemp;
    (void)output;
    (void)heaterState;
}

bool checkTemperatureSafetyLimits(float currentTemp, bool heaterState) {
    // Always return true for now
    (void)currentTemp;
    (void)heaterState;
    return true;
}

// PWM heater control functions - map to simple on/off control
void setHeaterDutyCycle(uint8_t dutyCycle) {
    // Map PWM duty cycle to simple on/off control
    if (dutyCycle > 50) {
        setBoilerOn();
    } else {
        setBoilerOff();
    }
}

void heaterHardwareOff(void) {
    // Turn heater off
    setBoilerOff();
}
