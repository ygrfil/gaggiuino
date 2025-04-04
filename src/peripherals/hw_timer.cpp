/* Hardware Timer Implementation for Heater Control */
#include "hw_timer.h"
#include "heater_control.h"  // Include the forward declarations for heater control functions
#include "../log.h"

// Define global variables that were declared with extern in header
HardwareTimer *heaterTimer = nullptr;
uint32_t heaterTimerChannel;
volatile uint8_t currentDutyCycle = 0;
bool timerSetupComplete = false;
bool timerInterruptsEnabled = false;

/**
 * Initialize the hardware timer for heater control
 * @return True if initialization successful
 */
bool heaterTimerInit() {
    // Use Timer 3 for heater control
    TIM_TypeDef *instance = TIM3;
    
    LOG_INFO("Setting up heater hardware timer with TIM3");
    
    // Initialize heater timer
    heaterTimer = new HardwareTimer(instance);
    if (!heaterTimer) {
        LOG_ERROR("Failed to create heater timer instance");
        return false;
    }
    
    // Configure the timer
    heaterTimer->setPrescaleFactor(TIMER_PRESCALER);
    heaterTimer->setOverflow(TIMER_PERIOD, MICROSEC_FORMAT);
    
    // Check if the pin supports PWM
    PinName pinName = digitalPinToPinName(relayPin);
    uint32_t function = pinmap_function(pinName, PinMap_PWM);
    
    // If the pin doesn't support PWM, we'll fall back to digital control
    if (function == NC) {
        LOG_ERROR("Pin %d does not support hardware PWM", relayPin);
        delete heaterTimer;
        heaterTimer = nullptr;
        // Return false to indicate hardware PWM is not available
        return false;
    }
    
    // Setup channel for PWM output on relayPin
    heaterTimerChannel = STM_PIN_CHANNEL(function);
    LOG_INFO("Using timer channel %d for PWM on pin %d", heaterTimerChannel, relayPin);
    
    heaterTimer->setMode(heaterTimerChannel, TIMER_OUTPUT_COMPARE_PWM1, relayPin);
    
    // Set default duty cycle to 0 (heater off)
    heaterTimer->setCaptureCompare(heaterTimerChannel, 0, PERCENT_COMPARE_FORMAT);
    
    // Start the timer
    heaterTimer->resume();
    
    timerSetupComplete = true;
    LOG_INFO("Hardware timer setup complete");
    return true;
}

/**
 * Set heater duty cycle
 * @param dutyCycle Duty cycle (0-100%)
 */
void setHeaterDutyCycle(uint8_t dutyCycle) {
    if (!timerSetupComplete || !heaterTimer) {
        // Fallback to digital control if hardware PWM is not available
        LOG_INFO("Hardware PWM not available, using direct control with duty cycle %d", dutyCycle);
        if (dutyCycle > 50) {
            digitalWrite(relayPin, HIGH);
        } else {
            digitalWrite(relayPin, LOW);
        }
        return;
    }
    
    // Constrain duty cycle to 0-100%
    currentDutyCycle = constrain(dutyCycle, 0, 100);
    
    // Set PWM duty cycle
    LOG_INFO("Setting heater PWM duty cycle to %d%%", currentDutyCycle);
    heaterTimer->setCaptureCompare(heaterTimerChannel, currentDutyCycle, PERCENT_COMPARE_FORMAT);
}

/**
 * Turn heater fully on (100% duty cycle)
 */
void heaterHardwareOn() {
    if (!timerSetupComplete || !heaterTimer) {
        // Fallback to direct pin control
        LOG_INFO("Hardware PWM not available, using direct pin control for ON");
        digitalWrite(relayPin, HIGH);
        return;
    }
    LOG_INFO("Setting heater to 100% duty cycle");
    setHeaterDutyCycle(100);
}

/**
 * Turn heater fully off (0% duty cycle)
 */
void heaterHardwareOff() {
    if (!timerSetupComplete || !heaterTimer) {
        // Fallback to direct pin control
        LOG_INFO("Hardware PWM not available, using direct pin control for OFF");
        digitalWrite(relayPin, LOW);
        return;
    }
    LOG_INFO("Setting heater to 0% duty cycle");
    setHeaterDutyCycle(0);
}

/**
 * Configure PWM pattern for heater
 * @param onTime Percentage of time heater is on (0-100%)
 */
void configureHeaterPWM(uint8_t onTime) {
    LOG_INFO("Configuring heater PWM with on-time %d%%", onTime);
    setHeaterDutyCycle(onTime);
}

/**
 * Clean up timer resources
 */
void heaterTimerCleanup() {
    if (heaterTimer) {
        LOG_INFO("Cleaning up heater timer resources");
        heaterTimer->pause();
        delete heaterTimer;
        heaterTimer = nullptr;
        timerSetupComplete = false;
    }
}

// Advanced PWM control for heater
void configurePWMHeaterControl(uint32_t pulseLength, int factor1, int factor2, bool brewActive) {
    if (!timerSetupComplete || !heaterTimer) {
        // Fallback to simple on/off control
        LOG_INFO("Hardware PWM not available, using simple on/off for brewing=%d", brewActive);
        if (brewActive) {
            setBoilerOn();
        } else {
            setBoilerOff();
        }
        return;
    }
    
    // Calculate period in microseconds
    uint32_t periodMicros = pulseLength * 1000; // Convert to microseconds
    
    // Calculate duty cycle based on factors and brew state
    uint32_t onTime, offTime;
    
    if (brewActive) {
        // During brewing - invert the control logic as required by justDoCoffee
        onTime = periodMicros / factor2;
        offTime = periodMicros * factor1;
    } else {
        // Normal operation
        onTime = periodMicros * factor1;
        offTime = periodMicros / factor2;
    }
    
    // Calculate duty cycle as percentage
    uint32_t totalCycleTime = onTime + offTime;
    uint8_t dutyCycle = (onTime * 100) / totalCycleTime;
    
    // Invert duty cycle for brew mode due to inverted logic in original code
    if (brewActive) {
        dutyCycle = 100 - dutyCycle;
    }
    
    // Set PWM frequency based on total cycle time
    uint32_t frequency = 1000000 / totalCycleTime; // in Hz
    
    // Ensure frequency is reasonable
    if (frequency < 1) frequency = 1;
    if (frequency > 10000) frequency = 10000;
    
    LOG_INFO("Setting PWM with pulse=%d, factor1=%d, factor2=%d, brew=%d, duty=%d%%", 
             pulseLength, factor1, factor2, brewActive, dutyCycle);
    
    // Reconfigure timer with new period
    heaterTimer->setPrescaleFactor(TIMER_PRESCALER);
    heaterTimer->setOverflow(totalCycleTime, MICROSEC_FORMAT);
    
    // Set the duty cycle
    setHeaterDutyCycle(dutyCycle);
}

// Map temperature to PWM duty cycle for heater control
uint8_t mapTemperatureToPWM(int16_t currentTemp, int16_t targetTemp, int16_t hysteresis) {
    // Scale values by 10 to maintain precision with integer math
    int16_t diff = targetTemp - currentTemp;
    
    // Simple proportional control implementation
    if (diff <= 0) {
        // At or above target temperature
        return 0;
    } else if (diff >= hysteresis) {
        // Far below target temperature
        return 100;
    } else {
        // Proportional control when within hysteresis range
        return (diff * 100) / hysteresis;
    }
}

// Apply hardware PWM control based on temperature parameters 
void applyHeaterControl(int16_t currentTemp, int16_t targetTemp, int16_t hysteresis, bool brewActive) {
    LOG_INFO("Temperature control: current=%d, target=%d, hysteresis=%d, brewing=%d", 
             currentTemp, targetTemp, hysteresis, brewActive);
             
    if (!timerSetupComplete || !heaterTimer) {
        // Fallback to simple on/off control if hardware PWM is not available
        LOG_INFO("Hardware PWM not available, using simple on/off based on temperature");
        if (targetTemp > currentTemp) {
            // Need heat
            LOG_INFO("Temperature below target, turning heater %s", brewActive ? "ON" : "OFF");
            brewActive ? setBoilerOn() : setBoilerOff();
        } else {
            // No heat needed
            LOG_INFO("Temperature at or above target, turning heater %s", brewActive ? "OFF" : "ON");
            brewActive ? setBoilerOff() : setBoilerOn();
        }
        return;
    }
    
    // Map temperature to appropriate PWM duty cycle
    uint8_t dutyCycle = mapTemperatureToPWM(currentTemp, targetTemp, hysteresis);
    
    // Invert duty cycle for brew mode
    if (brewActive) {
        dutyCycle = 100 - dutyCycle;
    }
    
    LOG_INFO("Temperature-based PWM duty cycle: %d%%", dutyCycle);
    
    // Apply the calculated duty cycle
    setHeaterDutyCycle(dutyCycle);
} 