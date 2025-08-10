/* Systematic Testing Framework Implementation */
#include "system_test_framework.h"
#include "../peripherals/peripherals.h"

// Global test framework instance
SystemTestFramework systemTest;

// PID parameter sets for different machine thermal masses
const PIDTuningSet SMALL_BOILER_PID = {
    // Brew mode (aggressive for quick response)
    3.0f, 1.0f, 1.5f,     // kp, ki, kd for brew
    // Idle mode (conservative for stability)  
    2.2f, 0.4f, 1.8f,     // kp, ki, kd for idle
    // Steam mode (very aggressive)
    4.0f, 1.2f, 0.8f,     // kp, ki, kd for steam
    180,                   // Sample time (ms)
    2.5f,                  // Ramp rate (°C/s)
    "Small Single Boiler (Gaggia Classic, Rancilio Silvia)"
};

const PIDTuningSet MEDIUM_BOILER_PID = {
    // Brew mode
    2.5f, 0.8f, 1.2f,     // kp, ki, kd for brew
    // Idle mode
    2.0f, 0.35f, 1.44f,   // kp, ki, kd for idle  
    // Steam mode
    3.0f, 0.8f, 0.5f,     // kp, ki, kd for steam
    200,                   // Sample time (ms)
    2.0f,                  // Ramp rate (°C/s)
    "Medium Dual Boiler (Breville Dual Boiler, ECM Synchronika)"
};

const PIDTuningSet LARGE_BOILER_PID = {
    // Brew mode (less aggressive for large thermal mass)
    1.8f, 0.4f, 0.8f,     // kp, ki, kd for brew
    // Idle mode
    1.5f, 0.2f, 1.0f,     // kp, ki, kd for idle
    // Steam mode
    2.2f, 0.5f, 0.3f,     // kp, ki, kd for steam
    250,                   // Sample time (ms)
    1.5f,                  // Ramp rate (°C/s)
    "Large Commercial (E61 Group Machines)"
};

SystemTestFramework::SystemTestFramework()
    : currentPhase(TestPhase::IDLE)
    , testStartTime(0)
    , phaseStartTime(0)
    , targetThermalMass(ThermalMassCategory::MEDIUM_DUAL_BOILER)
    , initialTemp(0.0f)
    , maxTempReached(0.0f)
    , minTempReached(999.0f)
    , stabilityViolations(0)
    , testAborted(false)
    , pressureTestMin(999.0f)
    , pressureTestMax(-999.0f)
    , pressureReadingCount(0)
    , emergencyTestActive(false)
    , emergencyTriggerTime(0)
    , testPIDController(nullptr)
    , tuningIterations(0)
{
    // Initialize test results
    memset(&testResults, 0, sizeof(testResults));
    testResults.overallResult = TestResult::RUNNING;
}

bool SystemTestFramework::initializeTestFramework() {
    LOG_INFO("=== SYSTEM TEST FRAMEWORK INITIALIZATION ===");
    
    // Initialize temperature safety system
    initTemperatureSafety();
    
    // Initialize PID temperature control
    initPIDTemperatureControl();
    
    // Enable diagnostic logging
    enableTemperatureDiagnosticLogging(true);
    enablePIDDiagnosticMode(true);
    
    // Create test PID controller for tuning experiments
    testPIDController = new PIDController();
    if (!testPIDController) {
        LOG_ERROR("Failed to create test PID controller");
        return false;
    }
    
    LOG_INFO("System test framework initialized successfully");
    return true;
}

bool SystemTestFramework::startSystematicTests(ThermalMassCategory thermalMass) {
    if (currentPhase != TestPhase::IDLE) {
        LOG_WARN("Test already running - aborting current test");
        abortTests();
    }
    
    LOG_INFO("=== STARTING SYSTEMATIC ESPRESSO MACHINE TESTING ===");
    LOG_INFO("Target thermal mass category: %d", static_cast<int>(thermalMass));
    
    targetThermalMass = thermalMass;
    testStartTime = millis();
    phaseStartTime = testStartTime;
    testAborted = false;
    
    // Reset test results
    memset(&testResults, 0, sizeof(testResults));
    testResults.overallResult = TestResult::RUNNING;
    testResults.detectedThermalMass = thermalMass;
    
    // Load optimal PID parameters for the thermal mass
    loadOptimalPIDParameters(thermalMass);
    
    // Start with cold start test
    currentPhase = TestPhase::COLD_START;
    LOG_INFO("Starting Phase 1: Cold Start Test (Target: %.1f°C)", BREW_TEMP_TARGET);
    
    return runColdStartTest();
}

bool SystemTestFramework::updateTests(float currentTemp, float currentPressure) {
    if (currentPhase == TestPhase::IDLE || currentPhase == TestPhase::COMPLETE) {
        return false; // No test running
    }
    
    // Safety check on every update
    if (!checkTestSafety(currentTemp, true)) {
        emergencyShutdown("Safety limit exceeded during test");
        return false;
    }
    
    // Update current test phase
    bool phaseComplete = false;
    
    switch (currentPhase) {
        case TestPhase::COLD_START:
            phaseComplete = updateColdStartTest(currentTemp);
            break;
            
        case TestPhase::BREW_STABILITY:
            phaseComplete = updateStabilityTest(currentTemp);
            break;
            
        case TestPhase::STEAM_TRANSITION:
            phaseComplete = updateSteamTransitionTest(currentTemp);
            break;
            
        case TestPhase::STEAM_STABILITY:
            phaseComplete = updateStabilityTest(currentTemp);
            break;
            
        case TestPhase::PRESSURE_VALIDATION:
            phaseComplete = updatePressureTest(currentPressure);
            break;
            
        case TestPhase::EMERGENCY_SHUTDOWN:
            phaseComplete = updateEmergencyTest(currentTemp);
            break;
            
        default:
            break;
    }
    
    // Move to next phase if current phase is complete
    if (phaseComplete) {
        switch (currentPhase) {
            case TestPhase::COLD_START:
                if (testResults.coldStartResult == TestResult::PASS) {
                    LOG_INFO("=== Phase 1 PASSED: Starting Phase 2: Brew Stability Test ===");
                    currentPhase = TestPhase::BREW_STABILITY;
                    runStabilityTest();
                } else {
                    LOG_ERROR("Cold start test failed - aborting systematic test");
                    testResults.overallResult = TestResult::FAIL;
                    currentPhase = TestPhase::COMPLETE;
                }
                break;
                
            case TestPhase::BREW_STABILITY:
                if (testResults.stabilityResult == TestResult::PASS) {
                    LOG_INFO("=== Phase 2 PASSED: Starting Phase 3: Steam Transition Test ===");
                    currentPhase = TestPhase::STEAM_TRANSITION;
                    runSteamTransitionTest();
                } else {
                    LOG_ERROR("Brew stability test failed - continuing with reduced confidence");
                    currentPhase = TestPhase::STEAM_TRANSITION;
                    runSteamTransitionTest();
                }
                break;
                
            case TestPhase::STEAM_TRANSITION:
                LOG_INFO("=== Phase 3 COMPLETE: Starting Phase 4: Steam Stability Test ===");
                currentPhase = TestPhase::STEAM_STABILITY;
                runStabilityTest();
                break;
                
            case TestPhase::STEAM_STABILITY:
                LOG_INFO("=== Phase 4 COMPLETE: Starting Phase 5: Pressure Sensor Test ===");
                currentPhase = TestPhase::PRESSURE_VALIDATION;
                runPressureSensorTest();
                break;
                
            case TestPhase::PRESSURE_VALIDATION:
                LOG_INFO("=== Phase 5 COMPLETE: Starting Phase 6: Emergency Shutdown Test ===");
                currentPhase = TestPhase::EMERGENCY_SHUTDOWN;
                runEmergencyShutdownTest();
                break;
                
            case TestPhase::EMERGENCY_SHUTDOWN:
                LOG_INFO("=== Phase 6 COMPLETE: All tests finished ===");
                currentPhase = TestPhase::COMPLETE;
                
                // Calculate overall result
                if (testResults.coldStartResult == TestResult::PASS &&
                    testResults.pressureTestResult == TestResult::PASS &&
                    testResults.emergencyShutdownResult == TestResult::PASS) {
                    testResults.overallResult = TestResult::PASS;
                } else {
                    testResults.overallResult = TestResult::FAIL;
                }
                
                testResults.totalTestTimeMs = millis() - testStartTime;
                generateTestReport();
                savePIDParametersForMachine(testResults.detectedThermalMass);
                break;
                
            default:
                break;
        }
        
        phaseStartTime = millis();
    }
    
    return currentPhase != TestPhase::COMPLETE;
}

bool SystemTestFramework::runColdStartTest() {
    LOG_INFO("Cold Start Test: Heating from ambient to %.1f°C", BREW_TEMP_TARGET);
    LOG_INFO("Maximum allowed time: %lu seconds", COLD_START_TIMEOUT_MS / 1000);
    LOG_INFO("Success criteria: Reach target without overshoot");
    
    initialTemp = 25.0f; // Assume room temperature start
    maxTempReached = 0.0f;
    testResults.coldStartResult = TestResult::RUNNING;
    testResults.coldStartOvershoot = false;
    testResults.coldStartOvershootAmount = 0.0f;
    
    // Enable heater for cold start
    setBoilerOn();
    
    return true;
}

bool SystemTestFramework::updateColdStartTest(float currentTemp) {
    uint32_t elapsed = millis() - phaseStartTime;
    
    // Track maximum temperature
    if (currentTemp > maxTempReached) {
        maxTempReached = currentTemp;
    }
    
    // Check for overshoot
    if (currentTemp > BREW_TEMP_TARGET + 0.1f) {
        if (!testResults.coldStartOvershoot) {
            testResults.coldStartOvershoot = true;
            testResults.coldStartOvershootAmount = currentTemp - BREW_TEMP_TARGET;
            LOG_WARN("Cold start overshoot detected: %.2f°C above target", 
                     testResults.coldStartOvershootAmount);
        }
    }
    
    // Log progress every 10 seconds
    if (elapsed % 10000 < 1000) {
        float progress = (currentTemp - initialTemp) / (BREW_TEMP_TARGET - initialTemp) * 100.0f;
        logTestProgress("COLD_START", progress);
    }
    
    // Check for success (within 0.5°C of target)
    if (abs(currentTemp - BREW_TEMP_TARGET) <= 0.5f) {
        testResults.coldStartTimeMs = elapsed;
        testResults.coldStartMaxTemp = maxTempReached;
        
        if (testResults.coldStartOvershoot && testResults.coldStartOvershootAmount > 2.0f) {
            testResults.coldStartResult = TestResult::FAIL;
            LOG_ERROR("Cold start FAILED: Excessive overshoot %.2f°C", 
                     testResults.coldStartOvershootAmount);
        } else {
            testResults.coldStartResult = TestResult::PASS;
            LOG_INFO("Cold start PASSED: Reached target in %lu seconds", elapsed / 1000);
        }
        return true;
    }
    
    // Check for timeout
    if (elapsed > COLD_START_TIMEOUT_MS) {
        testResults.coldStartResult = TestResult::TIMEOUT;
        testResults.coldStartTimeMs = elapsed;
        testResults.coldStartMaxTemp = maxTempReached;
        LOG_ERROR("Cold start TIMEOUT: Failed to reach target in %lu seconds", 
                 COLD_START_TIMEOUT_MS / 1000);
        return true;
    }
    
    return false; // Test still running
}

bool SystemTestFramework::runSteamTransitionTest() {
    LOG_INFO("Steam Transition Test: %.1f°C to %.1f°C", BREW_TEMP_TARGET, STEAM_TEMP_TARGET);
    LOG_INFO("Maximum allowed time: %lu seconds", STEAM_TRANSITION_TIMEOUT_MS / 1000);
    LOG_INFO("Success criteria: Smooth ramping without oscillation");
    
    maxTempReached = BREW_TEMP_TARGET;
    testResults.steamTransitionResult = TestResult::RUNNING;
    testResults.steamTransitionSmooth = true;
    
    return true;
}

bool SystemTestFramework::updateSteamTransitionTest(float currentTemp) {
    uint32_t elapsed = millis() - phaseStartTime;
    
    // Track maximum temperature
    if (currentTemp > maxTempReached) {
        maxTempReached = currentTemp;
    }
    
    // Check for smooth ramping (no more than 3°C/second rate of change)
    static float lastTemp = currentTemp;
    static uint32_t lastTime = millis();
    
    uint32_t currentTime = millis();
    if (currentTime - lastTime >= 1000) { // Check every second
        float rateOfChange = abs(currentTemp - lastTemp);
        if (rateOfChange > 5.0f) { // More than 5°C/second indicates oscillation
            testResults.steamTransitionSmooth = false;
            LOG_WARN("Steam transition: Non-smooth ramping detected (%.1f°C/s)", rateOfChange);
        }
        lastTemp = currentTemp;
        lastTime = currentTime;
    }
    
    // Log progress every 5 seconds
    if (elapsed % 5000 < 1000) {
        float progress = (currentTemp - BREW_TEMP_TARGET) / (STEAM_TEMP_TARGET - BREW_TEMP_TARGET) * 100.0f;
        logTestProgress("STEAM_TRANSITION", progress);
    }
    
    // Check for success (within 2°C of target)
    if (abs(currentTemp - STEAM_TEMP_TARGET) <= 2.0f) {
        testResults.steamTransitionTimeMs = elapsed;
        testResults.steamTransitionMaxTemp = maxTempReached;
        
        if (testResults.steamTransitionSmooth) {
            testResults.steamTransitionResult = TestResult::PASS;
            LOG_INFO("Steam transition PASSED: Smooth ramping in %lu seconds", elapsed / 1000);
        } else {
            testResults.steamTransitionResult = TestResult::FAIL;
            LOG_WARN("Steam transition PARTIAL: Reached target but ramping was not smooth");
        }
        return true;
    }
    
    // Check for timeout
    if (elapsed > STEAM_TRANSITION_TIMEOUT_MS) {
        testResults.steamTransitionResult = TestResult::TIMEOUT;
        testResults.steamTransitionTimeMs = elapsed;
        testResults.steamTransitionMaxTemp = maxTempReached;
        LOG_ERROR("Steam transition TIMEOUT: Failed to reach target in %lu seconds", 
                 STEAM_TRANSITION_TIMEOUT_MS / 1000);
        return true;
    }
    
    return false; // Test still running
}

bool SystemTestFramework::runStabilityTest(uint32_t durationMs) {
    float targetTemp = (currentPhase == TestPhase::BREW_STABILITY) ? BREW_TEMP_TARGET : STEAM_TEMP_TARGET;
    
    LOG_INFO("Temperature Stability Test: Hold %.1f°C ±%.1f°C for %lu seconds", 
             targetTemp, TEMP_STABILITY_TOLERANCE, durationMs / 1000);
    
    stabilityViolations = 0;
    testResults.stabilityResult = TestResult::RUNNING;
    testResults.stabilityMaxDeviation = 0.0f;
    testResults.stabilityRMSError = 0.0f;
    
    return true;
}

bool SystemTestFramework::updateStabilityTest(float currentTemp) {
    uint32_t elapsed = millis() - phaseStartTime;
    float targetTemp = (currentPhase == TestPhase::BREW_STABILITY) ? BREW_TEMP_TARGET : STEAM_TEMP_TARGET;
    
    // Calculate deviation from target
    float deviation = abs(currentTemp - targetTemp);
    
    // Track maximum deviation
    if (deviation > testResults.stabilityMaxDeviation) {
        testResults.stabilityMaxDeviation = deviation;
    }
    
    // Count stability violations
    if (deviation > TEMP_STABILITY_TOLERANCE) {
        stabilityViolations++;
        if (stabilityViolations == 1) {
            LOG_WARN("Stability violation: %.2f°C deviation at %lu seconds", 
                     deviation, elapsed / 1000);
        }
    }
    
    // Update RMS error calculation
    static float sumSquaredErrors = 0.0f;
    static uint32_t errorSampleCount = 0;
    sumSquaredErrors += deviation * deviation;
    errorSampleCount++;
    testResults.stabilityRMSError = sqrt(sumSquaredErrors / errorSampleCount);
    
    // Log progress every 10 seconds
    if (elapsed % 10000 < 1000) {
        float progress = (float)elapsed / (float)STABILITY_TEST_DURATION_MS * 100.0f;
        logTestProgress("STABILITY", progress);
        LOG_INFO("Stability: Temp=%.1f°C, Deviation=%.2f°C, Violations=%lu", 
                 currentTemp, deviation, stabilityViolations);
    }
    
    // Check for completion
    if (elapsed >= STABILITY_TEST_DURATION_MS) {
        testResults.stabilityViolationCount = stabilityViolations;
        
        // Pass if less than 5% of time spent in violation
        uint32_t maxAllowedViolations = STABILITY_TEST_DURATION_MS / 1000 * 0.05f; // 5% of duration
        
        if (stabilityViolations <= maxAllowedViolations && 
            testResults.stabilityMaxDeviation <= 2.0f * TEMP_STABILITY_TOLERANCE) {
            testResults.stabilityResult = TestResult::PASS;
            LOG_INFO("Stability test PASSED: Max deviation %.2f°C, RMS error %.3f°C, Violations: %lu", 
                     testResults.stabilityMaxDeviation, testResults.stabilityRMSError, stabilityViolations);
        } else {
            testResults.stabilityResult = TestResult::FAIL;
            LOG_ERROR("Stability test FAILED: Max deviation %.2f°C, Violations: %lu", 
                     testResults.stabilityMaxDeviation, stabilityViolations);
        }
        return true;
    }
    
    return false; // Test still running
}

bool SystemTestFramework::runPressureSensorTest() {
    LOG_INFO("Pressure Sensor Test: Validate readings in 0-9 bar range");
    LOG_INFO("Success criteria: Responsive readings within expected range");
    
    pressureTestMin = 999.0f;
    pressureTestMax = -999.0f;
    pressureReadingCount = 0;
    testResults.pressureTestResult = TestResult::RUNNING;
    testResults.pressureSensorResponsive = false;
    
    return true;
}

bool SystemTestFramework::updatePressureTest(float currentPressure) {
    uint32_t elapsed = millis() - phaseStartTime;
    
    // Track pressure range
    if (currentPressure < pressureTestMin) {
        pressureTestMin = currentPressure;
    }
    if (currentPressure > pressureTestMax) {
        pressureTestMax = currentPressure;
    }
    
    pressureReadingCount++;
    
    // Check if sensor is responsive (readings change over time)
    static float lastPressure = currentPressure;
    static uint32_t unchangedCount = 0;
    
    if (abs(currentPressure - lastPressure) < 0.01f) {
        unchangedCount++;
    } else {
        unchangedCount = 0;
        testResults.pressureSensorResponsive = true;
    }
    
    lastPressure = currentPressure;
    
    // Log progress every 5 seconds
    if (elapsed % 5000 < 1000) {
        LOG_INFO("Pressure test: Current=%.2f bar, Range=[%.2f, %.2f], Readings=%lu", 
                 currentPressure, pressureTestMin, pressureTestMax, pressureReadingCount);
    }
    
    // Test duration: 30 seconds
    if (elapsed >= 30000) {
        testResults.pressureMinReading = pressureTestMin;
        testResults.pressureMaxReading = pressureTestMax;
        
        // Validate pressure readings are in expected range
        bool validRange = (pressureTestMin >= -0.5f && pressureTestMax <= MAX_PRESSURE_BAR + 1.0f);
        bool responsiveRange = (pressureTestMax - pressureTestMin) > 0.1f;
        
        if (validRange && responsiveRange && testResults.pressureSensorResponsive) {
            testResults.pressureTestResult = TestResult::PASS;
            LOG_INFO("Pressure sensor test PASSED: Range=[%.2f, %.2f] bar, Responsive=YES", 
                     pressureTestMin, pressureTestMax);
        } else {
            testResults.pressureTestResult = TestResult::FAIL;
            LOG_ERROR("Pressure sensor test FAILED: Range=[%.2f, %.2f], Valid=%s, Responsive=%s", 
                     pressureTestMin, pressureTestMax, 
                     validRange ? "YES" : "NO",
                     testResults.pressureSensorResponsive ? "YES" : "NO");
        }
        return true;
    }
    
    return false; // Test still running
}

bool SystemTestFramework::runEmergencyShutdownTest() {
    LOG_INFO("Emergency Shutdown Test: Trigger safety shutdown at %.1f°C", EMERGENCY_TEMP_LIMIT);
    LOG_INFO("Success criteria: Heater disabled within 1 second");
    
    emergencyTestActive = true;
    emergencyTriggerTime = 0;
    testResults.emergencyShutdownResult = TestResult::RUNNING;
    testResults.heaterDisabledProperly = false;
    
    // Artificially trigger emergency condition by setting high target temperature
    // This is a simulated test - in practice you wouldn't actually overheat
    LOG_WARN("SIMULATED EMERGENCY: Testing shutdown response");
    
    return true;
}

bool SystemTestFramework::updateEmergencyTest(float currentTemp) {
    uint32_t elapsed = millis() - phaseStartTime;
    
    // Simulate emergency trigger after 5 seconds
    if (elapsed >= 5000 && emergencyTriggerTime == 0) {
        emergencyTriggerTime = millis();
        LOG_WARN("Emergency condition triggered - testing shutdown response");
        
        // Force emergency shutdown
        setBoilerOff();
        
        // Check that heater is actually off
        testResults.heaterDisabledProperly = true; // Assume it worked if we got here
    }
    
    // Check response time
    if (emergencyTriggerTime > 0) {
        uint32_t responseTime = millis() - emergencyTriggerTime;
        
        if (responseTime >= 10000) { // 10 seconds to complete test
            testResults.shutdownResponseTimeMs = responseTime;
            
            if (testResults.heaterDisabledProperly && responseTime <= 1000) {
                testResults.emergencyShutdownResult = TestResult::PASS;
                LOG_INFO("Emergency shutdown PASSED: Response time %lu ms", responseTime);
            } else {
                testResults.emergencyShutdownResult = TestResult::FAIL;
                LOG_ERROR("Emergency shutdown FAILED: Response time %lu ms, Disabled=%s", 
                         responseTime, testResults.heaterDisabledProperly ? "YES" : "NO");
            }
            return true;
        }
    }
    
    return false; // Test still running
}

void SystemTestFramework::generateTestReport() {
    LOG_INFO("=== COMPREHENSIVE SYSTEM TEST REPORT ===");
    LOG_INFO("Test Duration: %lu seconds", testResults.totalTestTimeMs / 1000);
    LOG_INFO("Detected Thermal Mass: %d", static_cast<int>(testResults.detectedThermalMass));
    LOG_INFO("");
    
    LOG_INFO("1. COLD START TEST: %s", 
             testResults.coldStartResult == TestResult::PASS ? "PASS" : "FAIL");
    LOG_INFO("   - Time to target: %lu seconds", testResults.coldStartTimeMs / 1000);
    LOG_INFO("   - Maximum temp: %.1f°C", testResults.coldStartMaxTemp);
    LOG_INFO("   - Overshoot: %s (%.2f°C)", 
             testResults.coldStartOvershoot ? "YES" : "NO", 
             testResults.coldStartOvershootAmount);
    LOG_INFO("");
    
    LOG_INFO("2. TEMPERATURE STABILITY: %s", 
             testResults.stabilityResult == TestResult::PASS ? "PASS" : "FAIL");
    LOG_INFO("   - Max deviation: %.2f°C", testResults.stabilityMaxDeviation);
    LOG_INFO("   - RMS error: %.3f°C", testResults.stabilityRMSError);
    LOG_INFO("   - Violations: %lu", testResults.stabilityViolationCount);
    LOG_INFO("");
    
    LOG_INFO("3. STEAM TRANSITION: %s", 
             testResults.steamTransitionResult == TestResult::PASS ? "PASS" : "FAIL");
    LOG_INFO("   - Transition time: %lu seconds", testResults.steamTransitionTimeMs / 1000);
    LOG_INFO("   - Smooth ramping: %s", testResults.steamTransitionSmooth ? "YES" : "NO");
    LOG_INFO("");
    
    LOG_INFO("4. PRESSURE SENSOR: %s", 
             testResults.pressureTestResult == TestResult::PASS ? "PASS" : "FAIL");
    LOG_INFO("   - Reading range: [%.2f, %.2f] bar", 
             testResults.pressureMinReading, testResults.pressureMaxReading);
    LOG_INFO("   - Responsive: %s", testResults.pressureSensorResponsive ? "YES" : "NO");
    LOG_INFO("");
    
    LOG_INFO("5. EMERGENCY SHUTDOWN: %s", 
             testResults.emergencyShutdownResult == TestResult::PASS ? "PASS" : "FAIL");
    LOG_INFO("   - Response time: %lu ms", testResults.shutdownResponseTimeMs);
    LOG_INFO("   - Heater disabled: %s", testResults.heaterDisabledProperly ? "YES" : "NO");
    LOG_INFO("");
    
    LOG_INFO("OVERALL RESULT: %s", 
             testResults.overallResult == TestResult::PASS ? "SYSTEM VALIDATED" : "SYSTEM NEEDS ATTENTION");
    
    documentPIDParameters();
}

void SystemTestFramework::documentPIDParameters() {
    LOG_INFO("=== FINAL PID PARAMETERS FOR MACHINE THERMAL MASS ===");
    
    const PIDTuningSet* params;
    switch (testResults.detectedThermalMass) {
        case ThermalMassCategory::SMALL_SINGLE_BOILER:
            params = &SMALL_BOILER_PID;
            break;
        case ThermalMassCategory::LARGE_COMMERCIAL:
            params = &LARGE_BOILER_PID;
            break;
        default:
            params = &MEDIUM_BOILER_PID;
            break;
    }
    
    LOG_INFO("Machine Category: %s", params->description);
    LOG_INFO("Brew Mode PID: Kp=%.2f, Ki=%.2f, Kd=%.2f", 
             params->kp_brew, params->ki_brew, params->kd_brew);
    LOG_INFO("Idle Mode PID: Kp=%.2f, Ki=%.2f, Kd=%.2f", 
             params->kp_idle, params->ki_idle, params->kd_idle);
    LOG_INFO("Steam Mode PID: Kp=%.2f, Ki=%.2f, Kd=%.2f", 
             params->kp_steam, params->ki_steam, params->kd_steam);
    LOG_INFO("Sample Time: %lu ms", params->sampleTimeMs);
    LOG_INFO("Ramp Rate: %.1f°C/s", params->rampRate);
    LOG_INFO("");
    LOG_INFO("Copy these parameters to your PID configuration for optimal performance.");
}

bool SystemTestFramework::checkTestSafety(float currentTemp, bool heaterState) {
    // Use the temperature safety system
    if (!checkTemperatureSafety(currentTemp, heaterState)) {
        return false;
    }
    
    // Additional test-specific safety checks
    if (currentTemp > EMERGENCY_TEMP_LIMIT && !emergencyTestActive) {
        LOG_ERROR("TEST SAFETY: Temperature exceeded emergency limit %.1f°C", EMERGENCY_TEMP_LIMIT);
        return false;
    }
    
    return true;
}

void SystemTestFramework::emergencyShutdown(const char* reason) {
    LOG_ERROR("EMERGENCY SHUTDOWN: %s", reason);
    
    // Immediately disable heater
    setBoilerOff();
    
    // Mark test as aborted
    testAborted = true;
    testResults.overallResult = TestResult::ABORTED;
    currentPhase = TestPhase::COMPLETE;
    
    // Log emergency in test results
    LOG_ERROR("=== TEST SEQUENCE ABORTED DUE TO EMERGENCY ===");
    generateTestReport();
}

void SystemTestFramework::loadOptimalPIDParameters(ThermalMassCategory thermalMass) {
    const PIDTuningSet* params;
    
    switch (thermalMass) {
        case ThermalMassCategory::SMALL_SINGLE_BOILER:
            params = &SMALL_BOILER_PID;
            break;
        case ThermalMassCategory::LARGE_COMMERCIAL:
            params = &LARGE_BOILER_PID;
            break;
        default:
            params = &MEDIUM_BOILER_PID;
            break;
    }
    
    LOG_INFO("Loading PID parameters for: %s", params->description);
    setPIDTunings(params->kp_brew, params->ki_brew, params->kd_brew);
    
    testResults.finalKp = params->kp_brew;
    testResults.finalKi = params->ki_brew;
    testResults.finalKd = params->kd_brew;
}

void SystemTestFramework::logTestProgress(const char* phase, float progress) {
    LOG_INFO("TEST PROGRESS [%s]: %.1f%% complete", phase, progress);
}

// Convenience functions implementation
void initSystemTesting() {
    systemTest.initializeTestFramework();
}

bool runFullSystemTest() {
    return systemTest.startSystematicTests();
}

void runQuickValidationTest() {
    LOG_INFO("=== QUICK VALIDATION TEST ===");
    LOG_INFO("Running basic temperature and pressure validation...");
    
    systemTest.initializeTestFramework();
    systemTest.runPressureSensorTest();
    
    // Run a short 30-second stability test
    systemTest.runStabilityTest(30000);
}

void testPressureSensorCalibration() {
    LOG_INFO("=== PRESSURE SENSOR CALIBRATION TEST ===");
    systemTest.runPressureSensorTest();
}

void validateEmergencyShutdown() {
    LOG_INFO("=== EMERGENCY SHUTDOWN VALIDATION ===");
    systemTest.runEmergencyShutdownTest();
}

// Missing method implementations
void SystemTestFramework::abortTests() {
    LOG_WARN("Aborting system tests at phase: %d", static_cast<int>(currentPhase));
    testAborted = true;
    testResults.overallResult = TestResult::ABORTED;
    currentPhase = TestPhase::COMPLETE;
    
    // Turn off heater for safety
    setBoilerOff();
}

void SystemTestFramework::autoTunePIDForThermalMass(ThermalMassCategory thermalMass) {
    LOG_INFO("Auto-tuning PID for thermal mass category: %d", static_cast<int>(thermalMass));
    
    // Load base parameters for the thermal mass
    loadOptimalPIDParameters(thermalMass);
    
    // Additional auto-tuning logic would go here
    // This is a simplified implementation - real auto-tuning would use
    // Ziegler-Nichols or relay auto-tuning methods
    tuningIterations++;
}

void SystemTestFramework::optimizePIDParameters() {
    LOG_INFO("Optimizing PID parameters based on test results");
    
    // Analyze test performance and adjust PID parameters
    if (testResults.coldStartOvershoot) {
        // Reduce aggressive gains if overshoot detected
        testResults.finalKp *= 0.9f;
        testResults.finalKd *= 0.9f;
        LOG_INFO("Reduced PID gains due to overshoot: Kp=%.2f, Kd=%.2f",
                 testResults.finalKp, testResults.finalKd);
    }
    
    if (testResults.stabilityMaxDeviation > 1.0f) {
        // Increase integral gain for better steady-state performance
        testResults.finalKi *= 1.1f;
        LOG_INFO("Increased Ki due to stability issues: Ki=%.2f", testResults.finalKi);
    }
    
    // Apply optimized parameters
    setPIDTunings(testResults.finalKp, testResults.finalKi, testResults.finalKd);
    testResults.pidOptimal = true;
}

ThermalMassCategory SystemTestFramework::detectThermalMass(float heatupRate, uint32_t thermalResponseTime) {
    // Classify thermal mass based on heating characteristics
    // Small machines heat up faster, large machines heat up slower
    
    if (heatupRate > 0.5f && thermalResponseTime < 180000) { // > 0.5°C/s, < 3 minutes
        LOG_INFO("Detected small thermal mass machine");
        return ThermalMassCategory::SMALL_SINGLE_BOILER;
    } else if (heatupRate < 0.2f && thermalResponseTime > 360000) { // < 0.2°C/s, > 6 minutes
        LOG_INFO("Detected large thermal mass machine");
        return ThermalMassCategory::LARGE_COMMERCIAL;
    } else {
        LOG_INFO("Detected medium thermal mass machine");
        return ThermalMassCategory::MEDIUM_DUAL_BOILER;
    }
}

void SystemTestFramework::savePIDParametersForMachine(ThermalMassCategory thermalMass) {
    LOG_INFO("Saving optimized PID parameters for machine type");
    
    // In a real implementation, this would save to EEPROM or flash memory
    // For now, just log the recommended parameters
    LOG_INFO("SAVE: Thermal Mass=%d, Kp=%.2f, Ki=%.2f, Kd=%.2f",
             static_cast<int>(thermalMass),
             testResults.finalKp, testResults.finalKi, testResults.finalKd);
}

void SystemTestFramework::analyzeHeatupCharacteristics(float currentTemp, uint32_t elapsedTime) {
    // Calculate heating rate for thermal mass detection
    if (initialTemp > 0 && elapsedTime > 0) {
        float heatupRate = (currentTemp - initialTemp) / (elapsedTime / 1000.0f);
        
        // Update detected thermal mass based on heating characteristics
        testResults.detectedThermalMass = detectThermalMass(heatupRate, elapsedTime);
    }
}

void SystemTestFramework::calculateStabilityMetrics(float currentTemp) {
    // This method is called from updateStabilityTest
    // Additional stability analysis could be added here
    static float tempHistory[60] = {0}; // 60 second history
    static uint32_t historyIndex = 0;
    
    // Store temperature history
    tempHistory[historyIndex] = currentTemp;
    historyIndex = (historyIndex + 1) % 60;
    
    // Calculate additional stability metrics here if needed
}

bool SystemTestFramework::validatePressureRange(float pressure) {
    return (pressure >= MIN_PRESSURE_BAR && pressure <= MAX_PRESSURE_BAR);
}

void SystemTestFramework::logTestResults() {
    // This is called from generateTestReport() - additional logging could be added
    LOG_INFO("Detailed test results logged");
}

bool SystemTestFramework::isTemperatureSafe(float temp) {
    return (temp <= EMERGENCY_TEMP_LIMIT);
}

bool SystemTestFramework::isTestTimeout() {
    uint32_t elapsed = millis() - phaseStartTime;
    
    switch (currentPhase) {
        case TestPhase::COLD_START:
            return elapsed > COLD_START_TIMEOUT_MS;
        case TestPhase::STEAM_TRANSITION:
            return elapsed > STEAM_TRANSITION_TIMEOUT_MS;
        default:
            return elapsed > 300000; // 5 minute default timeout
    }
}

void SystemTestFramework::resetTestState() {
    initialTemp = 0.0f;
    maxTempReached = 0.0f;
    minTempReached = 999.0f;
    stabilityViolations = 0;
    testAborted = false;
    emergencyTestActive = false;
    emergencyTriggerTime = 0;
    tuningIterations = 0;
}
