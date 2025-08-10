/* PID Controller for Temperature Control */
#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <Arduino.h>
#include "../log.h"

/**
 * Advanced PID Controller with Anti-Windup and Derivative Filtering
 * 
 * Features:
 * - Anti-windup protection to prevent integral saturation
 * - Derivative filtering to reduce noise impact
 * - Setpoint ramping to prevent overshoots on target changes
 * - Temperature rate limiting for safety
 * - Configurable output limits
 * - Debug logging for tuning
 */
class PIDController {
public:
    /**
     * Constructor
     * @param kp Proportional gain
     * @param ki Integral gain  
     * @param kd Derivative gain
     * @param outputMin Minimum output value
     * @param outputMax Maximum output value
     * @param sampleTime Sample time in milliseconds
     */
    PIDController(float kp = 2.0f, float ki = 0.5f, float kd = 1.0f, 
                  float outputMin = 0.0f, float outputMax = 100.0f, 
                  uint32_t sampleTime = 100);

    /**
     * Compute PID output
     * @param setpoint Target temperature
     * @param input Current temperature
     * @return PID output value (duty cycle 0-100%)
     */
    float compute(float setpoint, float input);

    /**
     * Set PID tuning parameters
     * @param kp Proportional gain
     * @param ki Integral gain
     * @param kd Derivative gain
     */
    void setTunings(float kp, float ki, float kd);

    /**
     * Set output limits
     * @param outputMin Minimum output value
     * @param outputMax Maximum output value
     */
    void setOutputLimits(float outputMin, float outputMax);

    /**
     * Set sample time
     * @param sampleTime Sample time in milliseconds
     */
    void setSampleTime(uint32_t sampleTime);

    /**
     * Enable/disable setpoint ramping
     * @param enabled Enable ramping
     * @param rampRate Maximum rate of change (°C/second)
     */
    void setSetpointRamping(bool enabled, float rampRate = 2.0f);

    /**
     * Enable/disable temperature rate limiting
     * @param enabled Enable rate limiting
     * @param maxRate Maximum temperature change rate (°C/second)
     */
    void setTemperatureRateLimiting(bool enabled, float maxRate = 5.0f);

    /**
     * Set derivative filter time constant
     * @param alpha Filter coefficient (0.0 = no filtering, 1.0 = maximum filtering)
     */
    void setDerivativeFilter(float alpha = 0.1f);

    /**
     * Reset PID controller state
     * Called when switching modes or after significant disruption
     */
    void reset();

    /**
     * Get current PID gains
     */
    void getTunings(float &kp, float &ki, float &kd) const;

    /**
     * Get current error value
     */
    float getError() const { return lastError; }

    /**
     * Get current integral term
     */
    float getIntegral() const { return integral; }

    /**
     * Get current derivative term  
     */
    float getDerivative() const { return derivative; }

    /**
     * Get last output value
     */
    float getOutput() const { return lastOutput; }

    /**
     * Enable/disable debug logging
     * @param enabled Enable debug output
     */
    void setDebugMode(bool enabled) { debugMode = enabled; }
    
    /**
     * Enable/disable diagnostic mode for temperature control testing
     * @param enabled Enable diagnostic output with detailed PID terms
     */
    void setDiagnosticMode(bool enabled) { diagnosticMode = enabled; }
    
    /**
     * Get current proportional term
     */
    float getProportionalTerm() const { return lastProportional; }
    
    /**
     * Detect temperature overshoot and return severity level
     * @param setpoint Target temperature
     * @param input Current temperature  
     * @return Overshoot severity (0=none, 1=mild, 2=moderate, 3=severe)
     */
    int detectOvershoot(float setpoint, float input);
    
    /**
     * Apply automatic PID detuning when overshoot is detected
     * @param overshootSeverity Severity level from detectOvershoot
     */
    void autoDetunePID(int overshootSeverity);
    
    /**
     * Log comprehensive diagnostic information for temperature control testing
     */
    void logDiagnosticInfo(float setpoint, float input, float output, float error);

private:
    // PID gains
    float kp, ki, kd;
    
    // Output limits
    float outputMin, outputMax;
    
    // Sample time in milliseconds
    uint32_t sampleTime;
    uint32_t lastTime;
    
    // PID terms
    float integral;
    float lastError;
    float derivative;
    float filteredDerivative;
    
    // Anti-windup protection
    bool useAntiWindup;
    float integralMin, integralMax;
    
    // Derivative filtering
    float derivativeAlpha;
    
    // Setpoint ramping
    bool useSetpointRamping;
    float rampRate;
    float rampedSetpoint;
    uint32_t lastRampTime;
    
    // Temperature rate limiting
    bool useTemperatureRateLimiting;
    float maxTemperatureRate;
    float lastValidInput;
    uint32_t lastInputTime;
    
    // State tracking
    float lastOutput;
    bool firstRun;
    
    // Debug mode
    bool debugMode;
    
    // Diagnostic mode for temperature control testing
    bool diagnosticMode;
    float lastProportional;
    
    // Overshoot detection
    float maxObservedTemperature;
    uint32_t overshootStartTime;
    bool overshootDetected;
    
    /**
     * Apply setpoint ramping
     * @param targetSetpoint Desired setpoint
     * @return Ramped setpoint
     */
    float applySetpointRamping(float targetSetpoint);
    
    /**
     * Apply temperature rate limiting
     * @param input Raw temperature input
     * @return Rate-limited temperature
     */
    float applyTemperatureRateLimiting(float input);
    
    /**
     * Calculate integral term with anti-windup
     * @param error Current error
     * @param output Current output before limiting
     * @return Integral term
     */
    float calculateIntegral(float error, float output);
    
    /**
     * Calculate derivative term with filtering
     * @param error Current error
     * @return Filtered derivative term
     */
    float calculateDerivative(float error);
    
    /**
     * Constrain output to limits
     * @param output Raw output value
     * @return Constrained output
     */
    float constrainOutput(float output);
    
    /**
     * Log debug information
     */
    void logDebugInfo(float setpoint, float input, float output, float error);
};

/**
 * Utility function to create a PID controller with conservative espresso machine settings
 * Suitable for temperature control with slow thermal response
 */
PIDController createEspressoTemperaturePID();

/**
 * Utility function to create a PID controller for steam temperature control
 * More aggressive settings for faster steam temperature response
 */
PIDController createSteamTemperaturePID();

#endif // PID_CONTROLLER_H
