/* Temperature Safety and Diagnostics System Implementation */
#include "temperature_safety.h"
#include "heater_control.h"

// Global instances
TemperatureSafetyMonitor temperatureSafety;
TemperatureStepTest stepTest;
TemperatureDiagnosticLogger diagnosticLogger;

// ============================================================================
// TemperatureSafetyMonitor Implementation
// ============================================================================

TemperatureSafetyMonitor::TemperatureSafetyMonitor() 
    : heaterCurrentlyOn(false)
    , continuousHeatingStartTime(0)
    , totalHeatingTime(0)
    , maxTemperatureRecorded(0.0f)
    , lastSafetyCheckTime(0)
    , safetyViolationActive(false)
{
}

bool TemperatureSafetyMonitor::checkSafety(float currentTemp, bool heaterState) {
    uint32_t currentTime = millis();
    
    // Update maximum temperature recorded
    if (currentTemp > maxTemperatureRecorded) {
        maxTemperatureRecorded = currentTemp;
    }
    
    // Track continuous heating time
    if (heaterState && !heaterCurrentlyOn) {
        // Heater just turned on
        continuousHeatingStartTime = currentTime;
        heaterCurrentlyOn = true;
        LOG_INFO("Safety: Heater turned ON, starting continuous heating timer");
    } else if (!heaterState && heaterCurrentlyOn) {
        // Heater just turned off
        uint32_t heatingDuration = currentTime - continuousHeatingStartTime;
        totalHeatingTime += heatingDuration;
        heaterCurrentlyOn = false;
        LOG_INFO("Safety: Heater turned OFF, heating duration: %lu ms, total: %lu ms", 
                 heatingDuration, totalHeatingTime);
    }
    
    // Check temperature safety limit
    if (currentTemp > MAX_SAFE_TEMPERATURE) {
        if (!safetyViolationActive) {
            logSafetyViolation("Temperature limit exceeded", currentTemp);
            safetyViolationActive = true;
        }
        // Emergency shutdown
        setBoilerOff();
        return false;
    }
    
    // Check continuous heating time limit
    if (heaterCurrentlyOn) {
        uint32_t continuousHeatingTime = currentTime - continuousHeatingStartTime;
        if (continuousHeatingTime > MAX_CONTINUOUS_HEATING_TIME) {
            if (!safetyViolationActive) {
                logSafetyViolation("Continuous heating time limit exceeded", currentTemp);
                safetyViolationActive = true;
            }
            // Emergency shutdown
            setBoilerOff();
            return false;
        }
    }
    
    // Reset safety violation flag if conditions are normal
    if (safetyViolationActive && currentTemp < MAX_SAFE_TEMPERATURE - 5.0f && !heaterCurrentlyOn) {
        safetyViolationActive = false;
        LOG_INFO("Safety: Safety violation cleared, normal operation resumed");
    }
    
    // Periodic safety status logging
    if (currentTime - lastSafetyCheckTime > 10000) {  // Every 10 seconds
        uint32_t remainingTime = getContinuousHeatingTimeRemaining();
        LOG_INFO("Safety Status - Temp: %.1f°C, Max: %.1f°C, Heater: %s, Remaining: %lu ms",
                 static_cast<double>(currentTemp), static_cast<double>(maxTemperatureRecorded),
                 heaterCurrentlyOn ? "ON" : "OFF", remainingTime);
        lastSafetyCheckTime = currentTime;
    }
    
    return true;  // Safe to continue
}

uint32_t TemperatureSafetyMonitor::getContinuousHeatingTimeRemaining() {
    if (!heaterCurrentlyOn) {
        return MAX_CONTINUOUS_HEATING_TIME;
    }
    
    uint32_t currentTime = millis();
    uint32_t elapsed = currentTime - continuousHeatingStartTime;
    
    if (elapsed >= MAX_CONTINUOUS_HEATING_TIME) {
        return 0;
    }
    
    return MAX_CONTINUOUS_HEATING_TIME - elapsed;
}

void TemperatureSafetyMonitor::resetContinuousHeatingTimer() {
    continuousHeatingStartTime = millis();
    totalHeatingTime = 0;
    LOG_INFO("Safety: Continuous heating timer reset");
}

void TemperatureSafetyMonitor::resetTemperatureStats() {
    maxTemperatureRecorded = 0.0f;
    totalHeatingTime = 0;
    safetyViolationActive = false;
    LOG_INFO("Safety: Temperature statistics reset");
}

void TemperatureSafetyMonitor::logSafetyViolation(const char* reason, float currentTemp) {
    LOG_ERROR("SAFETY VIOLATION: %s - Temperature: %.1f°C, Limit: %.1f°C", 
              reason, static_cast<double>(currentTemp), static_cast<double>(MAX_SAFE_TEMPERATURE));
}

// ============================================================================
// TemperatureStepTest Implementation
// ============================================================================

TemperatureStepTest::TemperatureStepTest()
    : testRunning(false)
    , initialSetpoint(0.0f)
    , stepTestTarget(0.0f)
    , stepSize(0.0f)
    , testStartTime(0)
    , settleStartTime(0)
    , initialTemp(0.0f)
    , maxTempReached(0.0f)
    , finalTemp(0.0f)
    , riseTimeResult(0)
    , settleTimeResult(0)
    , overshootResult(0.0f)
    , steadyStateErrorResult(0.0f)
    , resultsAvailable(false)
    , riseTimeMeasured(false)
    , overshootMeasured(false)
    , settleTimeMeasured(false)
{
}

bool TemperatureStepTest::startStepTest(float currentSetpoint, float stepSize) {
    if (testRunning) {
        LOG_WARN("Step test already running, stopping current test");
        stopStepTest();
    }
    
    initialSetpoint = currentSetpoint;
    this->stepSize = stepSize;
    stepTestTarget = currentSetpoint + stepSize;
    testStartTime = millis();
    settleStartTime = 0;
    
    // Reset measurement variables
    initialTemp = 0.0f;
    maxTempReached = 0.0f;
    finalTemp = 0.0f;
    riseTimeResult = 0;
    settleTimeResult = 0;
    overshootResult = 0.0f;
    steadyStateErrorResult = 0.0f;
    resultsAvailable = false;
    riseTimeMeasured = false;
    overshootMeasured = false;
    settleTimeMeasured = false;
    
    testRunning = true;
    
    LOG_INFO("Step Test Started: Initial: %.1f°C, Target: %.1f°C, Step: %.1f°C",
             static_cast<double>(initialSetpoint), static_cast<double>(stepTestTarget),
             static_cast<double>(stepSize));
    
    return true;
}

bool TemperatureStepTest::updateStepTest(float currentTemp, uint32_t currentTime) {
    if (!testRunning) {
        return false;
    }
    
    uint32_t elapsedTime = currentTime - testStartTime;
    
    // Store initial temperature (first reading)
    if (elapsedTime < 1000 && initialTemp == 0.0f) {
        initialTemp = currentTemp;
    }
    
    // Update maximum temperature reached
    if (currentTemp > maxTempReached) {
        maxTempReached = currentTemp;
    }
    
    // Measure rise time (63% of final value)
    if (!riseTimeMeasured && elapsedTime > STEP_TEST_SETTLE_TIME) {
        float targetChange = stepTestTarget - initialSetpoint;
        float currentChange = currentTemp - initialTemp;
        float progress = currentChange / targetChange;
        
        if (progress >= 0.63f) {
            riseTimeResult = elapsedTime;
            riseTimeMeasured = true;
            LOG_INFO("Step Test: Rise time measured: %lu ms (63%% reached)", riseTimeResult);
        }
    }
    
    // Check for test completion
    if (elapsedTime >= STEP_TEST_DURATION) {
        finalTemp = currentTemp;
        calculateResults();
        testRunning = false;
        LOG_INFO("Step Test Completed: Duration: %lu ms, Final: %.1f°C", elapsedTime, static_cast<double>(finalTemp));
        return false;
    }
    
    // Log progress every 5 seconds
    static uint32_t lastProgressLog = 0;
    if (currentTime - lastProgressLog >= 5000) {
        logStepTestProgress(currentTemp, currentTime);
        lastProgressLog = currentTime;
    }
    
    return true;
}

void TemperatureStepTest::stopStepTest() {
    if (testRunning) {
        testRunning = false;
        LOG_INFO("Step Test: Stopped manually");
    }
}

bool TemperatureStepTest::getStepTestResults(uint32_t& riseTime, uint32_t& settleTime,
                                           float& overshoot, float& steadyStateError) {
    if (!resultsAvailable) {
        return false;
    }
    
    riseTime = riseTimeResult;
    settleTime = settleTimeResult;
    overshoot = overshootResult;
    steadyStateError = steadyStateErrorResult;
    
    return true;
}

void TemperatureStepTest::calculateResults() {
    // Calculate overshoot
    float expectedFinalTemp = stepTestTarget;
    
    // Safety check: prevent division by zero
    if (fabsf(expectedFinalTemp) < 0.1f) {
        LOG_WARN("Step Test: Invalid target temperature (%.2f°C), cannot calculate overshoot", 
                 static_cast<double>(expectedFinalTemp));
        overshootResult = 0.0f;
    } else {
        if (stepSize > 0) {
            overshootResult = ((maxTempReached - expectedFinalTemp) / expectedFinalTemp) * 100.0f;
        } else {
            overshootResult = ((expectedFinalTemp - maxTempReached) / expectedFinalTemp) * 100.0f;
        }
    }
    
    // Calculate steady-state error
    steadyStateErrorResult = stepTestTarget - finalTemp;
    
    // Estimate settle time (time to stay within 2% of final value)
    // This is a simplified calculation - in practice, you'd need more sophisticated analysis
    settleTimeResult = riseTimeResult * 3;  // Rough estimate
    
    resultsAvailable = true;
    
    LOG_INFO("Step Test Results - Rise: %lu ms, Settle: %lu ms, Overshoot: %.1f%%, Error: %.2f°C",
             riseTimeResult, settleTimeResult, static_cast<double>(overshootResult), 
             static_cast<double>(steadyStateErrorResult));
}

void TemperatureStepTest::logStepTestProgress(float currentTemp, uint32_t currentTime) {
    uint32_t elapsed = currentTime - testStartTime;
    float progress = (float)elapsed / (float)STEP_TEST_DURATION * 100.0f;
    
    LOG_INFO("Step Test Progress: %.1f%% - Temp: %.1f°C, Target: %.1f°C, Max: %.1f°C",
             static_cast<double>(progress), static_cast<double>(currentTemp),
             static_cast<double>(stepTestTarget), static_cast<double>(maxTempReached));
}

// ============================================================================
// TemperatureDiagnosticLogger Implementation  
// ============================================================================

TemperatureDiagnosticLogger::TemperatureDiagnosticLogger()
    : loggingEnabled(false)
    , lastLogTime(0)
    , logInterval(1000)  // 1 second interval
    , logCounter(0)
{
}

void TemperatureDiagnosticLogger::logDataPoint(uint32_t timestamp, float setpoint, float actual,
                                             float pidOutput, bool heaterState,
                                             float pidP, float pidI, float pidD) {
    if (!loggingEnabled || (timestamp - lastLogTime < logInterval)) {
        return;
    }
    
    LOG_INFO("TEMP_DATA,%lu,%.2f,%.2f,%.1f,%d,%.3f,%.3f,%.3f",
             timestamp, static_cast<double>(setpoint), static_cast<double>(actual),
             static_cast<double>(pidOutput), heaterState ? 1 : 0,
             static_cast<double>(pidP), static_cast<double>(pidI), static_cast<double>(pidD));
    
    lastLogTime = timestamp;
    logCounter++;
    
    // Log summary every 60 data points
    if (logCounter % 60 == 0) {
        LOG_INFO("Diagnostic: Logged %lu temperature data points", logCounter);
    }
}

void TemperatureDiagnosticLogger::logStepTestDataPoint(uint32_t timestamp, float setpoint, 
                                                      float actual, float pidOutput, 
                                                      const char* testPhase) {
    if (!loggingEnabled) {
        return;
    }
    
    LOG_INFO("STEP_TEST_DATA,%lu,%.2f,%.2f,%.1f,%s",
             timestamp, static_cast<double>(setpoint), static_cast<double>(actual),
             static_cast<double>(pidOutput), testPhase);
}

// ============================================================================
// Convenience Functions
// ============================================================================

void initTemperatureSafety() {
    temperatureSafety.resetTemperatureStats();
    LOG_INFO("Temperature Safety System initialized");
}

bool checkTemperatureSafety(float currentTemp, bool heaterState) {
    return temperatureSafety.checkSafety(currentTemp, heaterState);
}

void startTemperatureStepTest(float currentSetpoint) {
    stepTest.startStepTest(currentSetpoint);
}

void updateTemperatureStepTest(float currentTemp) {
    if (stepTest.isStepTestRunning()) {
        stepTest.updateStepTest(currentTemp, millis());
    }
}

void enableTemperatureDiagnosticLogging(bool enabled) {
    diagnosticLogger.setLoggingEnabled(enabled);
    LOG_INFO("Temperature diagnostic logging %s", enabled ? "enabled" : "disabled");
}

void logTemperatureDiagnostics(float setpoint, float actual, float pidOutput,
                              bool heaterState, float pidP, float pidI, float pidD) {
    diagnosticLogger.logDataPoint(millis(), setpoint, actual, pidOutput, heaterState, pidP, pidI, pidD);
    
    // Also log step test data if test is running
    if (stepTest.isStepTestRunning()) {
        diagnosticLogger.logStepTestDataPoint(millis(), setpoint, actual, pidOutput, "RUNNING");
    }
}
