/* PID Controller Implementation */
#include "pid_controller.h"

PIDController::PIDController(float kp, float ki, float kd, 
                           float outputMin, float outputMax, 
                           uint32_t sampleTime)
    : kp(kp), ki(ki), kd(kd)
    , outputMin(outputMin), outputMax(outputMax)
    , sampleTime(sampleTime)
    , lastTime(0)
    , integral(0.0f)
    , lastError(0.0f)
    , derivative(0.0f)
    , filteredDerivative(0.0f)
    , useAntiWindup(true)
    , integralMin(-50.0f)  // Conservative integral limits
    , integralMax(50.0f)
    , derivativeAlpha(0.1f)  // 10% derivative filtering
    , useSetpointRamping(true)
    , rampRate(2.0f)  // 2°C/second max ramping
    , rampedSetpoint(0.0f)
    , lastRampTime(0)
    , useTemperatureRateLimiting(true)
    , maxTemperatureRate(5.0f)  // 5°C/second max temperature change
    , lastValidInput(0.0f)
    , lastInputTime(0)
    , lastOutput(0.0f)
    , firstRun(true)
    , debugMode(false)
    , diagnosticMode(false)
    , lastProportional(0.0f)
    , maxObservedTemperature(0.0f)
    , overshootStartTime(0)
    , overshootDetected(false)
{
    LOG_INFO("PID Controller initialized: Kp=%.2f, Ki=%.2f, Kd=%.2f", 
             static_cast<double>(kp), static_cast<double>(ki), static_cast<double>(kd));
}

float PIDController::compute(float setpoint, float input) {
    uint32_t currentTime = millis();
    
    // Check if enough time has passed for next calculation
    if (currentTime - lastTime < sampleTime) {
        return lastOutput;
    }
    
    // Apply temperature rate limiting for safety
    if (useTemperatureRateLimiting) {
        input = applyTemperatureRateLimiting(input);
    }
    
    // Apply setpoint ramping to prevent overshoots
    float effectiveSetpoint = setpoint;
    if (useSetpointRamping) {
        effectiveSetpoint = applySetpointRamping(setpoint);
    }
    
    // Calculate time delta in seconds
    float deltaTime = (currentTime - lastTime) / 1000.0f;
    
    // Calculate error
    float error = effectiveSetpoint - input;
    
    // Calculate proportional term
    float proportional = kp * error;
    lastProportional = proportional;  // Store for diagnostics
    
    // Calculate integral term with anti-windup
    float rawOutput = proportional + integral + derivative;
    integral = calculateIntegral(error, rawOutput);
    
    // Calculate derivative term with filtering
    derivative = calculateDerivative(error);
    
    // Calculate final output
    float output = proportional + integral + derivative;
    
    // Constrain output to limits
    output = constrainOutput(output);
    
    // Debug logging if enabled
    if (debugMode) {
        logDebugInfo(effectiveSetpoint, input, output, error);
    }
    
    // Diagnostic logging if enabled
    if (diagnosticMode) {
        logDiagnosticInfo(effectiveSetpoint, input, output, error);
    }
    
    // Store values for next iteration
    lastError = error;
    lastOutput = output;
    lastTime = currentTime;
    firstRun = false;
    
    return output;
}

void PIDController::setTunings(float newKp, float newKi, float newKd) {
    if (newKp < 0 || newKi < 0 || newKd < 0) {
        LOG_ERROR("PID: Invalid tuning parameters - values must be non-negative");
        return;
    }
    
    kp = newKp;
    ki = newKi;
    kd = newKd;
    
    LOG_INFO("PID tunings updated: Kp=%.2f, Ki=%.2f, Kd=%.2f", 
             static_cast<double>(kp), static_cast<double>(ki), static_cast<double>(kd));
}

void PIDController::setOutputLimits(float newOutputMin, float newOutputMax) {
    if (newOutputMax < newOutputMin) {
        LOG_ERROR("PID: Invalid output limits - max must be >= min");
        return;
    }
    
    outputMin = newOutputMin;
    outputMax = newOutputMax;
    
    // Adjust integral limits proportionally
    float range = outputMax - outputMin;
    integralMin = -range * 0.5f;
    integralMax = range * 0.5f;
    
    // Constrain existing integral
    integral = constrain(integral, integralMin, integralMax);
    
    LOG_INFO("PID output limits set: min=%.1f, max=%.1f", 
             static_cast<double>(outputMin), static_cast<double>(outputMax));
}

void PIDController::setSampleTime(uint32_t newSampleTime) {
    if (newSampleTime < 10) {
        LOG_ERROR("PID: Sample time too small, minimum is 10ms");
        return;
    }
    
    // Adjust integral and derivative gains for new sample time
    float ratio = (float)newSampleTime / (float)sampleTime;
    ki *= ratio;
    kd /= ratio;
    
    sampleTime = newSampleTime;
    
    LOG_INFO("PID sample time set to %dms", sampleTime);
}

void PIDController::setSetpointRamping(bool enabled, float rampRateVal) {
    useSetpointRamping = enabled;
    if (rampRateVal > 0) {
        rampRate = rampRateVal;
    }
    
    LOG_INFO("PID setpoint ramping %s, rate=%.1f°C/s", 
             enabled ? "enabled" : "disabled", static_cast<double>(rampRate));
}

void PIDController::setTemperatureRateLimiting(bool enabled, float maxRate) {
    useTemperatureRateLimiting = enabled;
    if (maxRate > 0) {
        maxTemperatureRate = maxRate;
    }
    
    LOG_INFO("PID temperature rate limiting %s, max rate=%.1f°C/s", 
             enabled ? "enabled" : "disabled", static_cast<double>(maxTemperatureRate));
}

void PIDController::setDerivativeFilter(float alpha) {
    derivativeAlpha = constrain(alpha, 0.0f, 1.0f);
    LOG_INFO("PID derivative filter alpha set to %.2f", static_cast<double>(derivativeAlpha));
}

void PIDController::reset() {
    integral = 0.0f;
    lastError = 0.0f;
    derivative = 0.0f;
    filteredDerivative = 0.0f;
    lastOutput = 0.0f;
    firstRun = true;
    lastTime = 0;
    lastRampTime = 0;
    lastInputTime = 0;
    
    LOG_INFO("PID controller reset");
}

void PIDController::getTunings(float &outKp, float &outKi, float &outKd) const {
    outKp = kp;
    outKi = ki;
    outKd = kd;
}

float PIDController::applySetpointRamping(float targetSetpoint) {
    uint32_t currentTime = millis();
    
    if (firstRun || lastRampTime == 0) {
        rampedSetpoint = targetSetpoint;
        lastRampTime = currentTime;
        return targetSetpoint;
    }
    
    float deltaTime = (currentTime - lastRampTime) / 1000.0f;
    float maxChange = rampRate * deltaTime;
    
    float difference = targetSetpoint - rampedSetpoint;
    
    if (abs(difference) <= maxChange) {
        rampedSetpoint = targetSetpoint;
    } else {
        rampedSetpoint += (difference > 0) ? maxChange : -maxChange;
    }
    
    lastRampTime = currentTime;
    return rampedSetpoint;
}

float PIDController::applyTemperatureRateLimiting(float input) {
    uint32_t currentTime = millis();
    
    if (firstRun || lastInputTime == 0) {
        lastValidInput = input;
        lastInputTime = currentTime;
        return input;
    }
    
    float deltaTime = (currentTime - lastInputTime) / 1000.0f;
    
    // Prevent division by zero
    if (deltaTime <= 0) {
        return lastValidInput;
    }
    
    float maxChange = maxTemperatureRate * deltaTime;
    float difference = input - lastValidInput;
    
    // Apply rate limiting
    float limitedInput;
    if (abs(difference) <= maxChange) {
        limitedInput = input;
    } else {
        limitedInput = lastValidInput + ((difference > 0) ? maxChange : -maxChange);
        LOG_WARN("PID: Temperature rate limited from %.1f to %.1f", 
                 static_cast<double>(input), static_cast<double>(limitedInput));
    }
    
    lastValidInput = limitedInput;
    lastInputTime = currentTime;
    return limitedInput;
}

float PIDController::calculateIntegral(float error, float rawOutput) {
    if (!useAntiWindup) {
        return integral + (ki * error * sampleTime / 1000.0f);
    }
    
    // Anti-windup: Only accumulate integral if output is not saturated
    // or if the integral would help reduce saturation
    float proposedIntegral = integral + (ki * error * sampleTime / 1000.0f);
    
    // Check if output would be saturated
    bool outputSaturated = (rawOutput <= outputMin) || (rawOutput >= outputMax);
    
    if (!outputSaturated) {
        // Not saturated, use proposed integral
        return constrain(proposedIntegral, integralMin, integralMax);
    }
    
    // Output is saturated, check if integral would help
    bool integralHelps = false;
    if (rawOutput <= outputMin && error > 0) {
        integralHelps = true;  // Positive error when output at minimum
    } else if (rawOutput >= outputMax && error < 0) {
        integralHelps = true;  // Negative error when output at maximum
    }
    
    if (integralHelps) {
        return constrain(proposedIntegral, integralMin, integralMax);
    }
    
    // Integral would make saturation worse, don't update
    return integral;
}

float PIDController::calculateDerivative(float error) {
    if (firstRun) {
        return 0.0f;
    }
    
    float deltaTime = sampleTime / 1000.0f;
    float rawDerivative = kd * (error - lastError) / deltaTime;
    
    // Apply derivative filtering to reduce noise
    filteredDerivative = (derivativeAlpha * filteredDerivative) + 
                        ((1.0f - derivativeAlpha) * rawDerivative);
    
    return filteredDerivative;
}

float PIDController::constrainOutput(float output) {
    return constrain(output, outputMin, outputMax);
}

void PIDController::logDebugInfo(float setpoint, float input, float output, float error) {
    static uint32_t lastDebugTime = 0;
    uint32_t currentTime = millis();
    
    // Limit debug output to once per second
    if (currentTime - lastDebugTime >= 1000) {
        LOG_INFO("PID Debug - SP:%.1f IN:%.1f OUT:%.1f ERR:%.2f P:%.2f I:%.2f D:%.2f", 
                 static_cast<double>(setpoint), static_cast<double>(input), 
                 static_cast<double>(output), static_cast<double>(error),
                 static_cast<double>(kp * error), static_cast<double>(integral), 
                 static_cast<double>(derivative));
        lastDebugTime = currentTime;
    }
}

// Utility functions for creating pre-configured PID controllers

PIDController createEspressoTemperaturePID() {
    // Conservative settings for espresso temperature control
    // Espresso machines have slow thermal response, so we need stable control
    PIDController pid(2.0f, 0.5f, 1.0f, 0.0f, 100.0f, 200);  // 200ms sample time
    
    // Enable safety features
    pid.setSetpointRamping(true, 1.5f);  // 1.5°C/second max ramp rate
    pid.setTemperatureRateLimiting(true, 3.0f);  // 3°C/second max temp change
    pid.setDerivativeFilter(0.15f);  // Moderate derivative filtering
    
    LOG_INFO("Created espresso temperature PID controller");
    return pid;
}

PIDController createSteamTemperaturePID() {
    // More aggressive settings for steam temperature control
    // Steam needs faster response but still stable
    PIDController pid(3.0f, 0.8f, 0.5f, 0.0f, 100.0f, 150);  // 150ms sample time
    
    // Enable safety features with higher rates for steam
    pid.setSetpointRamping(true, 3.0f);  // 3°C/second max ramp rate
    pid.setTemperatureRateLimiting(true, 8.0f);  // 8°C/second max temp change
    pid.setDerivativeFilter(0.2f);  // More derivative filtering for steam
    
    LOG_INFO("Created steam temperature PID controller");
    return pid;
}

// Diagnostic Functions Implementation

int PIDController::detectOvershoot(float setpoint, float input) {
    // Update maximum observed temperature
    if (input > maxObservedTemperature) {
        maxObservedTemperature = input;
    }
    
    // Check for overshoot condition
    float overshoot = input - setpoint;
    
    if (overshoot > 0.1f) {  // Threshold for overshoot detection
        if (!overshootDetected) {
            overshootDetected = true;
            overshootStartTime = millis();
            LOG_WARN("PID: Overshoot detected - Current: %.2f°C, Setpoint: %.2f°C, Overshoot: %.2f°C",
                     static_cast<double>(input), static_cast<double>(setpoint), static_cast<double>(overshoot));
        }
        
        // Classify overshoot severity
        if (overshoot >= 5.0f) {
            return 3;  // Severe overshoot
        } else if (overshoot >= 2.0f) {
            return 2;  // Moderate overshoot
        } else if (overshoot >= 0.5f) {
            return 1;  // Mild overshoot
        }
    } else {
        // Reset overshoot detection when temperature comes back within range
        if (overshootDetected && overshoot <= 0) {
            overshootDetected = false;
            LOG_INFO("PID: Overshoot condition cleared");
        }
    }
    
    return 0;  // No overshoot
}

void PIDController::autoDetunePID(int overshootSeverity) {
    if (overshootSeverity == 0) {
        return;  // No detuning needed
    }
    
    float detuningFactor;
    switch (overshootSeverity) {
        case 1:  // Mild overshoot
            detuningFactor = 0.9f;
            break;
        case 2:  // Moderate overshoot
            detuningFactor = 0.8f;
            break;
        case 3:  // Severe overshoot
            detuningFactor = 0.7f;
            break;
        default:
            detuningFactor = 0.9f;
            break;
    }
    
    // Store original tunings for logging
    float originalKp = kp;
    float originalKi = ki;
    float originalKd = kd;
    
    // Apply detuning - reduce aggressive gains
    kp *= detuningFactor;
    ki *= detuningFactor * 0.8f;  // Reduce integral gain more aggressively
    kd *= detuningFactor;
    
    LOG_WARN("PID: Auto-detuning applied (severity %d) - Kp: %.2f->%.2f, Ki: %.2f->%.2f, Kd: %.2f->%.2f",
             overshootSeverity,
             static_cast<double>(originalKp), static_cast<double>(kp),
             static_cast<double>(originalKi), static_cast<double>(ki),
             static_cast<double>(originalKd), static_cast<double>(kd));
}

void PIDController::logDiagnosticInfo(float setpoint, float input, float output, float error) {
    static uint32_t lastDiagnosticTime = 0;
    uint32_t currentTime = millis();
    
    // Log diagnostic info every 500ms for detailed analysis
    if (currentTime - lastDiagnosticTime >= 500) {
        // Check for overshoot and apply auto-detuning if enabled
        int overshootLevel = detectOvershoot(setpoint, input);
        if (overshootLevel > 0) {
            autoDetunePID(overshootLevel);
        }
        
        // Comprehensive diagnostic logging
        LOG_INFO("PID DIAGNOSTIC - Time:%lu SP:%.2f IN:%.2f OUT:%.1f ERR:%.3f P:%.3f I:%.3f D:%.3f Max:%.2f OS:%d",
                 currentTime,
                 static_cast<double>(setpoint),
                 static_cast<double>(input),
                 static_cast<double>(output),
                 static_cast<double>(error),
                 static_cast<double>(lastProportional),
                 static_cast<double>(integral),
                 static_cast<double>(derivative),
                 static_cast<double>(maxObservedTemperature),
                 overshootLevel);
                 
        lastDiagnosticTime = currentTime;
    }
}
