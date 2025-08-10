/* Temperature Safety and Diagnostics System */
#ifndef TEMPERATURE_SAFETY_H
#define TEMPERATURE_SAFETY_H

#include <Arduino.h>
#include "../log.h"

// Safety limits
const float MAX_SAFE_TEMPERATURE = 110.0f;        // Maximum safe temperature (°C)
const uint32_t MAX_CONTINUOUS_HEATING_TIME = 30000;  // Maximum continuous heating time (ms)

// Temperature step test parameters
const float STEP_TEST_TARGET_OFFSET = 10.0f;      // Temperature step size for testing (°C)
const uint32_t STEP_TEST_DURATION = 60000;        // Step test duration (ms)
const uint32_t STEP_TEST_SETTLE_TIME = 5000;      // Time to settle before measuring response (ms)

/**
 * Temperature Safety Monitor
 * Monitors temperature limits and heating time to prevent dangerous conditions
 */
class TemperatureSafetyMonitor {
public:
    TemperatureSafetyMonitor();
    
    /**
     * Check safety conditions and take action if limits are exceeded
     * @param currentTemp Current temperature reading (°C)
     * @param heaterState Current heater state (true = ON)
     * @return true if safe to continue, false if safety limit exceeded
     */
    bool checkSafety(float currentTemp, bool heaterState);
    
    /**
     * Get time remaining before continuous heating limit is reached
     * @return Time remaining in milliseconds, 0 if heater is off or limit exceeded
     */
    uint32_t getContinuousHeatingTimeRemaining();
    
    /**
     * Reset continuous heating timer (call when heater is turned off)
     */
    void resetContinuousHeatingTimer();
    
    /**
     * Get maximum temperature recorded since last reset
     */
    float getMaxTemperatureRecorded() const { return maxTemperatureRecorded; }
    
    /**
     * Reset temperature monitoring statistics
     */
    void resetTemperatureStats();
    
private:
    bool heaterCurrentlyOn;
    uint32_t continuousHeatingStartTime;
    uint32_t totalHeatingTime;
    float maxTemperatureRecorded;
    uint32_t lastSafetyCheckTime;
    bool safetyViolationActive;
    
    void logSafetyViolation(const char* reason, float currentTemp);
};

/**
 * Temperature Step Response Test
 * Performs step response testing for PID tuning analysis
 */
class TemperatureStepTest {
public:
    TemperatureStepTest();
    
    /**
     * Start a temperature step response test
     * @param currentSetpoint Current temperature setpoint
     * @param stepSize Size of temperature step (positive or negative)
     * @return true if test started successfully
     */
    bool startStepTest(float currentSetpoint, float stepSize = STEP_TEST_TARGET_OFFSET);
    
    /**
     * Update step test with current temperature reading
     * @param currentTemp Current temperature (°C)
     * @param currentTime Current system time (millis())
     * @return true if test is still running, false if completed
     */
    bool updateStepTest(float currentTemp, uint32_t currentTime);
    
    /**
     * Check if step test is currently running
     */
    bool isStepTestRunning() const { return testRunning; }
    
    /**
     * Get current step test target temperature
     */
    float getStepTestTarget() const { return stepTestTarget; }
    
    /**
     * Stop current step test
     */
    void stopStepTest();
    
    /**
     * Get step test results
     * @param riseTime Time to reach 63% of final value (ms)
     * @param settleTime Time to settle within 2% of final value (ms)
     * @param overshoot Maximum overshoot percentage
     * @param steadyStateError Final steady-state error (°C)
     * @return true if results are available
     */
    bool getStepTestResults(uint32_t& riseTime, uint32_t& settleTime, 
                           float& overshoot, float& steadyStateError);
    
private:
    bool testRunning;
    float initialSetpoint;
    float stepTestTarget;
    float stepSize;
    uint32_t testStartTime;
    uint32_t settleStartTime;
    
    // Measurement data
    float initialTemp;
    float maxTempReached;
    float finalTemp;
    uint32_t riseTimeResult;
    uint32_t settleTimeResult;
    float overshootResult;
    float steadyStateErrorResult;
    bool resultsAvailable;
    
    // State tracking
    bool riseTimeMeasured;
    bool overshootMeasured;
    bool settleTimeMeasured;
    
    void calculateResults();
    void logStepTestProgress(float currentTemp, uint32_t currentTime);
};

/**
 * Diagnostic Data Logger
 * Logs temperature control data for analysis
 */
class TemperatureDiagnosticLogger {
public:
    TemperatureDiagnosticLogger();
    
    /**
     * Enable/disable diagnostic logging
     */
    void setLoggingEnabled(bool enabled) { loggingEnabled = enabled; }
    
    /**
     * Log temperature control data point
     * @param timestamp Current time (millis())
     * @param setpoint Target temperature (°C)
     * @param actual Current temperature (°C)
     * @param pidOutput PID controller output (0-100%)
     * @param heaterState Heater state (true = ON)
     * @param pidP Proportional term
     * @param pidI Integral term
     * @param pidD Derivative term
     */
    void logDataPoint(uint32_t timestamp, float setpoint, float actual, 
                     float pidOutput, bool heaterState,
                     float pidP, float pidI, float pidD);
    
    /**
     * Log step test data point with additional context
     */
    void logStepTestDataPoint(uint32_t timestamp, float setpoint, float actual, 
                             float pidOutput, const char* testPhase);
    
private:
    bool loggingEnabled;
    uint32_t lastLogTime;
    uint32_t logInterval;
    uint32_t logCounter;
};

// Global instances
extern TemperatureSafetyMonitor temperatureSafety;
extern TemperatureStepTest stepTest;
extern TemperatureDiagnosticLogger diagnosticLogger;

// Convenience functions
void initTemperatureSafety();
bool checkTemperatureSafety(float currentTemp, bool heaterState);
void startTemperatureStepTest(float currentSetpoint);
void updateTemperatureStepTest(float currentTemp);
void enableTemperatureDiagnosticLogging(bool enabled);
void logTemperatureDiagnostics(float setpoint, float actual, float pidOutput, 
                              bool heaterState, float pidP, float pidI, float pidD);

#endif // TEMPERATURE_SAFETY_H
