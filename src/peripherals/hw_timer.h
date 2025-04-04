/* Hardware Timer for Heater Control */
#ifndef HW_TIMER_H
#define HW_TIMER_H

#include <Arduino.h>
#include <HardwareTimer.h>
#include "../pindef.h"

// Timer instance for heater control PWM
extern HardwareTimer *heaterTimer;

// Timer channel for the heater pin
extern uint32_t heaterTimerChannel;

// Default values used for timer configuration
const uint32_t TIMER_PRESCALER = 1; // No prescaling
const uint32_t TIMER_PERIOD = 1000; // 1000 clock ticks = 1ms @1000Hz
const uint32_t TIMER_FREQUENCY = 1000; // 1kHz PWM frequency

// Current PWM duty cycle (0-100%)
extern volatile uint8_t currentDutyCycle;

// Timer setup complete flag
extern bool timerSetupComplete;

// Timer interrupts are enabled
extern bool timerInterruptsEnabled;

/**
 * Initialize the hardware timer for heater control
 * @return True if initialization successful
 */
bool heaterTimerInit();

/**
 * Set heater duty cycle
 * @param dutyCycle Duty cycle (0-100%)
 */
void setHeaterDutyCycle(uint8_t dutyCycle);

/**
 * Turn heater fully on (100% duty cycle)
 */
void heaterHardwareOn();

/**
 * Turn heater fully off (0% duty cycle)
 */
void heaterHardwareOff();

/**
 * Configure PWM pattern for heater
 * @param onTime Percentage of time heater is on (0-100%)
 */
void configureHeaterPWM(uint8_t onTime);

/**
 * Clean up timer resources
 */
void heaterTimerCleanup();

/**
 * Advanced PWM control for heater using factors
 * 
 * @param pulseLength Base pulse length in milliseconds
 * @param factor1 First timing factor
 * @param factor2 Second timing factor
 * @param brewActive Whether brewing is active (changes timing logic)
 */
void configurePWMHeaterControl(uint32_t pulseLength, int factor1, int factor2, bool brewActive);

/**
 * Map temperature to PWM duty cycle for heater control
 * 
 * @param currentTemp Current temperature (scaled by 10)
 * @param targetTemp Target temperature (scaled by 10)
 * @param hysteresis Temperature hysteresis range (scaled by 10)
 * @return PWM duty cycle (0-100%)
 */
uint8_t mapTemperatureToPWM(int16_t currentTemp, int16_t targetTemp, int16_t hysteresis);

/**
 * Apply heater control based on temperature
 * 
 * @param currentTemp Current temperature (scaled by 10)
 * @param targetTemp Target temperature (scaled by 10)
 * @param hysteresis Temperature hysteresis range (scaled by 10)
 * @param brewActive Whether brewing is active
 */
void applyHeaterControl(int16_t currentTemp, int16_t targetTemp, int16_t hysteresis, bool brewActive);

#endif // HW_TIMER_H 