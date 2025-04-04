/* Hardware Timer Implementation for Heater Control */
#include "hw_timer.h"

/**
 * Initialize the hardware timer for heater control
 * @return True if initialization successful
 */
bool heaterTimerInit() {
    // Use Timer 3 for heater control
    TIM_TypeDef *instance = TIM3;
    heaterTimer = new HardwareTimer(instance);
    
    if (!heaterTimer) return false;
    
    // Configure the timer
    heaterTimer->setPrescaleFactor(TIMER_PRESCALER);
    heaterTimer->setOverflow(TIMER_PERIOD, MICROSEC_FORMAT);
    
    // Setup channel for PWM output on relayPin
    heaterTimerChannel = STM_PIN_CHANNEL(pinmap_function(digitalPinToPinName(relayPin), PinMap_PWM));
    heaterTimer->setMode(heaterTimerChannel, TIMER_OUTPUT_COMPARE_PWM1, relayPin);
    
    // Set default duty cycle to 0 (heater off)
    heaterTimer->setCaptureCompare(heaterTimerChannel, 0, PERCENT_COMPARE_FORMAT);
    
    // Start the timer
    heaterTimer->resume();
    
    timerSetupComplete = true;
    return true;
}

/**
 * Set heater duty cycle
 * @param dutyCycle Duty cycle (0-100%)
 */
void setHeaterDutyCycle(uint8_t dutyCycle) {
    if (!timerSetupComplete) return;
    
    // Constrain duty cycle to 0-100%
    currentDutyCycle = constrain(dutyCycle, 0, 100);
    
    // Set PWM duty cycle
    heaterTimer->setCaptureCompare(heaterTimerChannel, currentDutyCycle, PERCENT_COMPARE_FORMAT);
}

/**
 * Turn heater fully on (100% duty cycle)
 */
void heaterHardwareOn() {
    setHeaterDutyCycle(100);
}

/**
 * Turn heater fully off (0% duty cycle)
 */
void heaterHardwareOff() {
    setHeaterDutyCycle(0);
}

/**
 * Configure PWM pattern for heater
 * @param onTime Percentage of time heater is on (0-100%)
 */
void configureHeaterPWM(uint8_t onTime) {
    setHeaterDutyCycle(onTime);
}

/**
 * Clean up timer resources
 */
void heaterTimerCleanup() {
    if (heaterTimer) {
        heaterTimer->pause();
        delete heaterTimer;
        heaterTimer = nullptr;
        timerSetupComplete = false;
    }
}

// Advanced PWM control for heater
void configurePWMHeaterControl(uint32_t pulseLength, int factor1, int factor2, bool brewActive) {
    if (!timerSetupComplete) return;
    
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
    if (!timerSetupComplete) return;
    
    // Map temperature to appropriate PWM duty cycle
    uint8_t dutyCycle = mapTemperatureToPWM(currentTemp, targetTemp, hysteresis);
    
    // Invert duty cycle for brew mode
    if (brewActive) {
        dutyCycle = 100 - dutyCycle;
    }
    
    // Apply the calculated duty cycle
    setHeaterDutyCycle(dutyCycle);
} 