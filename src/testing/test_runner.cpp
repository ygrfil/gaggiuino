/* Test Runner Application for System Testing Framework
 * 
 * This demonstrates how to use the comprehensive testing framework
 * to validate espresso machine control system performance.
 * 
 * Usage:
 * 1. Call initSystemTesting() during setup
 * 2. Call runFullSystemTest() to start comprehensive testing
 * 3. Monitor test progress and results
 * 4. Apply recommended PID parameters based on results
 */

#include "system_test_framework.h"
#include "../peripherals/peripherals.h"
#include "../../lib/Common/sensors_state.h"

// Test runner state
static bool testingActive = false;
static uint32_t lastTestUpdate = 0;
static float simulatedTemperature = 25.0f; // Room temperature start
static float simulatedPressure = 0.0f;

// Forward declarations
void handleTestCompletion();
void provideRecommendations(const SystemTestResults& results);
void showNextSteps(const SystemTestResults& results);

// Simulated sensor readings for demonstration
float getSimulatedTemperature() {
    // Simple temperature simulation for demo purposes
    static uint32_t lastTempUpdate = 0;
    uint32_t now = millis();
    
    if (now - lastTempUpdate >= 1000) { // Update every second
        if (systemTest.getCurrentPhase() == TestPhase::COLD_START) {
            // Simulate heating up to brew temperature
            if (simulatedTemperature < BREW_TEMP_TARGET) {
                simulatedTemperature += 0.8f; // 0.8°C per second heating rate
            }
        } else if (systemTest.getCurrentPhase() == TestPhase::STEAM_TRANSITION) {
            // Simulate heating up to steam temperature
            if (simulatedTemperature < STEAM_TEMP_TARGET) {
                simulatedTemperature += 0.6f; // Slower heating to steam temp
            }
        } else if (systemTest.getCurrentPhase() == TestPhase::BREW_STABILITY ||
                   systemTest.getCurrentPhase() == TestPhase::STEAM_STABILITY) {
            // Simulate stable temperature with small variations
            float targetTemp = (systemTest.getCurrentPhase() == TestPhase::BREW_STABILITY) ? 
                              BREW_TEMP_TARGET : STEAM_TEMP_TARGET;
            float error = targetTemp - simulatedTemperature;
            simulatedTemperature += error * 0.1f; // Simple proportional control simulation
            
            // Add small random variations
            simulatedTemperature += (random(-10, 10) / 100.0f); // ±0.1°C random variation
        }
        lastTempUpdate = now;
    }
    
    return simulatedTemperature;
}

float getSimulatedPressure() {
    // Simple pressure simulation
    if (systemTest.getCurrentPhase() == TestPhase::PRESSURE_VALIDATION) {
        // Simulate pressure changes during validation
        uint32_t elapsed = (millis() / 5000) % 6; // 6 phases of 5 seconds each
        
        switch (elapsed) {
            case 0: simulatedPressure = 0.0f; break;    // Atmospheric
            case 1: simulatedPressure = 2.5f; break;    // Pre-infusion
            case 2: simulatedPressure = 6.0f; break;    // Brewing
            case 3: simulatedPressure = 9.0f; break;    // Full pressure
            case 4: simulatedPressure = 4.0f; break;    // Declining
            case 5: simulatedPressure = 0.2f; break;    // Residual
        }
    }
    
    return simulatedPressure;
}

/**
 * Initialize the test runner system
 */
void initTestRunner() {
    LOG_INFO("=== ESPRESSO MACHINE TEST RUNNER INITIALIZATION ===");
    
    // Initialize peripherals
    pinInit();
    
    // Initialize system testing framework
    initSystemTesting();
    
    LOG_INFO("Test runner initialized successfully");
    LOG_INFO("Use runComprehensiveTest() to start full system validation");
}

/**
 * Run comprehensive system test with automatic thermal mass detection
 */
bool runComprehensiveTest() {
    LOG_INFO("=== STARTING COMPREHENSIVE ESPRESSO MACHINE TEST ===");
    LOG_INFO("This test will validate all critical control systems:");
    LOG_INFO("1. Cold start temperature control (93°C target)");
    LOG_INFO("2. Temperature stability (±0.5°C for 30 seconds)");
    LOG_INFO("3. Steam mode transition (93°C to 140°C)");
    LOG_INFO("4. Pressure sensor validation (0-9 bar range)");
    LOG_INFO("5. Emergency shutdown response");
    LOG_INFO("6. PID parameter optimization");
    LOG_INFO("");
    LOG_INFO("Test will take approximately 8-15 minutes depending on machine type.");
    LOG_INFO("Monitor the log output for detailed progress and results.");
    LOG_INFO("");
    
    // Start comprehensive testing with medium thermal mass as default
    // The system will auto-detect and adjust during testing
    bool started = systemTest.startSystematicTests(ThermalMassCategory::MEDIUM_DUAL_BOILER);
    
    if (started) {
        testingActive = true;
        lastTestUpdate = millis();
        LOG_INFO("✓ Comprehensive test started successfully");
        LOG_INFO("Monitor test progress below...");
        return true;
    } else {
        LOG_ERROR("✗ Failed to start comprehensive test");
        return false;
    }
}

/**
 * Run quick validation test (subset of full test suite)
 */
void runQuickTest() {
    LOG_INFO("=== QUICK SYSTEM VALIDATION TEST ===");
    LOG_INFO("Running abbreviated test sequence for basic validation...");
    
    runQuickValidationTest();
    
    LOG_INFO("Quick test completed. For full validation, use runComprehensiveTest()");
}

/**
 * Run individual test components
 */
void runColdStartTest() {
    LOG_INFO("=== COLD START TEST ONLY ===");
    systemTest.initializeTestFramework();
    systemTest.startSystematicTests(ThermalMassCategory::MEDIUM_DUAL_BOILER);
    testingActive = true;
}

void runPressureTest() {
    LOG_INFO("=== PRESSURE SENSOR TEST ONLY ===");
    testPressureSensorCalibration();
}

void runEmergencyTest() {
    LOG_INFO("=== EMERGENCY SHUTDOWN TEST ONLY ===");
    validateEmergencyShutdown();
}

/**
 * Update test execution (call from main loop)
 */
void updateTestRunner() {
    if (!testingActive) {
        return;
    }
    
    uint32_t now = millis();
    
    // Update test every 500ms
    if (now - lastTestUpdate >= 500) {
        // Get current sensor readings
        float currentTemp = getSimulatedTemperature(); // Replace with actual temperature sensor
        float currentPressure = getSimulatedPressure(); // Replace with actual pressure sensor
        
        // Update test framework
        bool stillRunning = systemTest.updateTests(currentTemp, currentPressure);
        
        if (!stillRunning) {
            // Test completed
            testingActive = false;
            handleTestCompletion();
        }
        
        lastTestUpdate = now;
    }
}

/**
 * Handle test completion and report results
 */
void handleTestCompletion() {
    LOG_INFO("");
    LOG_INFO("=== TEST SEQUENCE COMPLETED ===");
    
    const SystemTestResults& results = systemTest.getTestResults();
    
    // Display summary results
    if (results.overallResult == TestResult::PASS) {
        LOG_INFO("🎉 CONGRATULATIONS! Your espresso machine has PASSED all tests!");
        LOG_INFO("The control system is properly calibrated and operating safely.");
    } else if (results.overallResult == TestResult::FAIL) {
        LOG_ERROR("⚠️  Some tests FAILED. Your system needs attention before use.");
        LOG_ERROR("Please review the detailed results and address any issues.");
    } else if (results.overallResult == TestResult::ABORTED) {
        LOG_ERROR("🛑 Testing was ABORTED due to safety concerns.");
        LOG_ERROR("DO NOT USE the machine until issues are resolved.");
    }
    
    LOG_INFO("");
    
    // Provide specific recommendations
    provideRecommendations(results);
    
    // Show next steps
    showNextSteps(results);
}

/**
 * Provide specific recommendations based on test results
 */
void provideRecommendations(const SystemTestResults& results) {
    LOG_INFO("=== RECOMMENDATIONS ===");
    
    // Cold start recommendations
    if (results.coldStartResult == TestResult::FAIL) {
        LOG_WARN("Cold Start Issues:");
        if (results.coldStartOvershoot) {
            LOG_WARN("- Reduce PID aggressiveness (lower Kp and Kd)");
            LOG_WARN("- Check heater thermal coupling");
        }
        if (results.coldStartTimeMs > COLD_START_TIMEOUT_MS * 0.8f) {
            LOG_WARN("- Slow heating: Check heater element and power supply");
            LOG_WARN("- Consider increasing PID proportional gain");
        }
    }
    
    // Stability recommendations
    if (results.stabilityResult == TestResult::FAIL) {
        LOG_WARN("Stability Issues:");
        if (results.stabilityMaxDeviation > 1.0f) {
            LOG_WARN("- Large temperature swings: Tune PID parameters");
            LOG_WARN("- Check temperature sensor placement and thermal mass");
        }
        if (results.stabilityViolationCount > 5) {
            LOG_WARN("- Frequent violations: Increase derivative filtering");
            LOG_WARN("- Consider increasing integral gain for steady-state");
        }
    }
    
    // Pressure sensor recommendations
    if (results.pressureTestResult == TestResult::FAIL) {
        LOG_ERROR("Pressure Sensor Issues:");
        if (!results.pressureSensorResponsive) {
            LOG_ERROR("- Sensor not responding: Check wiring and connections");
        }
        if (results.pressureMinReading < -1.0f || results.pressureMaxReading > 12.0f) {
            LOG_ERROR("- Invalid readings: Calibrate or replace pressure sensor");
        }
    }
    
    // Emergency shutdown recommendations
    if (results.emergencyShutdownResult == TestResult::FAIL) {
        LOG_ERROR("Safety Issues:");
        LOG_ERROR("- Emergency shutdown failed: Check safety systems immediately");
        LOG_ERROR("- DO NOT USE machine until safety systems are verified");
    }
}

/**
 * Show next steps based on test results
 */
void showNextSteps(const SystemTestResults& results) {
    LOG_INFO("");
    LOG_INFO("=== NEXT STEPS ===");
    
    if (results.overallResult == TestResult::PASS) {
        LOG_INFO("✓ Your machine is ready for use!");
        LOG_INFO("✓ PID parameters have been optimized for your thermal mass");
        LOG_INFO("✓ All safety systems are functioning properly");
        LOG_INFO("");
        LOG_INFO("Recommended actions:");
        LOG_INFO("1. Save the PID parameters to your configuration");
        LOG_INFO("2. Run periodic validation tests (monthly)");
        LOG_INFO("3. Monitor temperature stability during regular use");
    } else {
        LOG_WARN("⚠️  Action required before using your machine:");
        LOG_WARN("1. Address all FAILED test components");
        LOG_WARN("2. Re-run tests after making corrections");
        LOG_WARN("3. Do not use machine until all tests PASS");
        
        if (results.emergencyShutdownResult == TestResult::FAIL) {
            LOG_ERROR("");
            LOG_ERROR("🛑 SAFETY CRITICAL: Emergency shutdown failed");
            LOG_ERROR("🛑 Machine is UNSAFE - do not operate until fixed");
        }
    }
    
    LOG_INFO("");
    LOG_INFO("For support, save the complete log output and contact:");
    LOG_INFO("- Gaggiuino community forums");
    LOG_INFO("- GitHub issues page");
    LOG_INFO("- Local espresso machine technician");
}

/**
 * Display current test status (call periodically for status updates)
 */
void displayTestStatus() {
    if (!testingActive) {
        LOG_INFO("No test currently running. Use runComprehensiveTest() to start.");
        return;
    }
    
    TestPhase currentPhase = systemTest.getCurrentPhase();
    const char* phaseNames[] = {
        "Idle", "Cold Start", "Brew Stability", "Steam Transition",
        "Steam Stability", "Pressure Validation", "Emergency Shutdown", "Complete"
    };
    
    LOG_INFO("Current test phase: %s", phaseNames[static_cast<int>(currentPhase)]);
    
    // Show phase-specific status
    switch (currentPhase) {
        case TestPhase::COLD_START:
            LOG_INFO("Target: %.1f°C, Current: %.1f°C", 
                     BREW_TEMP_TARGET, getSimulatedTemperature());
            break;
        case TestPhase::STEAM_TRANSITION:
            LOG_INFO("Target: %.1f°C, Current: %.1f°C", 
                     STEAM_TEMP_TARGET, getSimulatedTemperature());
            break;
        case TestPhase::PRESSURE_VALIDATION:
            LOG_INFO("Current pressure: %.2f bar", getSimulatedPressure());
            break;
        default:
            break;
    }
}

/**
 * Abort current test sequence
 */
void abortCurrentTest() {
    if (testingActive) {
        LOG_WARN("Aborting current test sequence...");
        systemTest.abortTests();
        testingActive = false;
        LOG_WARN("Test sequence aborted by user");
    } else {
        LOG_INFO("No test currently running");
    }
}

/**
 * Get current test results (for external monitoring)
 */
const SystemTestResults& getCurrentTestResults() {
    return systemTest.getTestResults();
}

/**
 * Check if testing is currently active
 */
bool isTestingActive() {
    return testingActive;
}

// Example usage function that could be called from main()
void exampleTestUsage() {
    LOG_INFO("=== EXAMPLE: How to use the testing framework ===\n");
    
    // Initialize the test runner
    initTestRunner();
    
    // Option 1: Run comprehensive test
    LOG_INFO("Option 1: Comprehensive test (recommended)");
    LOG_INFO("runComprehensiveTest();");
    LOG_INFO("while(isTestingActive()) { updateTestRunner(); delay(100); }");
    LOG_INFO("");
    
    // Option 2: Run quick validation
    LOG_INFO("Option 2: Quick validation");
    LOG_INFO("runQuickTest();");
    LOG_INFO("");
    
    // Option 3: Individual tests
    LOG_INFO("Option 3: Individual tests");
    LOG_INFO("runColdStartTest(); // Test heating performance");
    LOG_INFO("runPressureTest();  // Test pressure sensor");
    LOG_INFO("runEmergencyTest(); // Test safety shutdown");
    LOG_INFO("");
    
    LOG_INFO("Integration with main loop:");
    LOG_INFO("void loop() {");
    LOG_INFO("    updateTestRunner(); // Call this every loop iteration");
    LOG_INFO("    // ... your other code");
    LOG_INFO("}");
    LOG_INFO("");
}
