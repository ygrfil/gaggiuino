/* Systematic Testing Framework for Espresso Machine Control System
 * 
 * This framework implements comprehensive testing procedures to validate:
 * - Cold start to brew temperature performance
 * - Steam mode transition smoothness  
 * - Temperature stability during shots
 * - Pressure sensor validation
 * - Emergency shutdown functionality
 * - PID parameter documentation for different thermal masses
 */

#ifndef SYSTEM_TEST_FRAMEWORK_H
#define SYSTEM_TEST_FRAMEWORK_H

#include <Arduino.h>
#include "../log.h"
#include "../peripherals/pid_controller.h"
#include "../peripherals/temperature_safety.h"
#include "../peripherals/heater_control.h"

// Test temperature targets
const float BREW_TEMP_TARGET = 93.0f;        // 93°C brew temperature
const float STEAM_TEMP_TARGET = 140.0f;      // 140°C steam temperature
const float TEMP_STABILITY_TOLERANCE = 0.5f; // ±0.5°C stability requirement
const uint32_t SHOT_DURATION_MS = 30000;     // 30-second shot duration
const float MIN_PRESSURE_BAR = 0.0f;         // Minimum pressure
const float MAX_PRESSURE_BAR = 9.0f;         // Maximum typical shot pressure

// Test timeout and safety limits
const uint32_t COLD_START_TIMEOUT_MS = 300000;   // 5 minutes max for cold start
const uint32_t STEAM_TRANSITION_TIMEOUT_MS = 120000; // 2 minutes max for steam transition
const uint32_t STABILITY_TEST_DURATION_MS = 60000;   // 1 minute stability test
const float EMERGENCY_TEMP_LIMIT = 115.0f;   // Emergency shutdown temperature

// Test result status
enum class TestResult {
    PASS,
    FAIL,
    TIMEOUT,
    ABORTED,
    RUNNING
};

// Test phases
enum class TestPhase {
    IDLE,
    COLD_START,
    BREW_STABILITY,
    STEAM_TRANSITION,  
    STEAM_STABILITY,
    PRESSURE_VALIDATION,
    EMERGENCY_SHUTDOWN,
    COMPLETE
};

// Machine thermal mass categories for PID tuning
enum class ThermalMassCategory {
    SMALL_SINGLE_BOILER,    // Gaggia Classic, Rancilio Silvia
    MEDIUM_DUAL_BOILER,     // Breville Dual Boiler, ECM Synchronika
    LARGE_COMMERCIAL        // Commercial E61 group machines
};

/**
 * Comprehensive test results structure
 */
struct SystemTestResults {
    // Cold start test
    TestResult coldStartResult;
    uint32_t coldStartTimeMs;
    float coldStartMaxTemp;
    bool coldStartOvershoot;
    float coldStartOvershootAmount;
    
    // Steam transition test
    TestResult steamTransitionResult;
    uint32_t steamTransitionTimeMs;
    float steamTransitionMaxTemp;
    bool steamTransitionSmooth;
    
    // Temperature stability test
    TestResult stabilityResult;
    float stabilityMaxDeviation;
    float stabilityRMSError;
    uint32_t stabilityViolationCount;
    
    // Pressure sensor test
    TestResult pressureTestResult;
    float pressureMinReading;
    float pressureMaxReading;
    bool pressureSensorResponsive;
    
    // Emergency shutdown test
    TestResult emergencyShutdownResult;
    uint32_t shutdownResponseTimeMs;
    bool heaterDisabledProperly;
    
    // Overall test result
    TestResult overallResult;
    uint32_t totalTestTimeMs;
    ThermalMassCategory detectedThermalMass;
    
    // PID performance metrics
    float finalKp, finalKi, finalKd;
    uint32_t pidAutoTuneIterations;
    bool pidOptimal;
};

/**
 * PID tuning recommendations for different thermal masses
 */
struct PIDTuningSet {
    float kp_brew, ki_brew, kd_brew;
    float kp_idle, ki_idle, kd_idle;  
    float kp_steam, ki_steam, kd_steam;
    uint32_t sampleTimeMs;
    float rampRate;
    const char* description;
};

/**
 * Main system testing class
 */
class SystemTestFramework {
public:
    SystemTestFramework();
    
    /**
     * Initialize the testing framework
     */
    bool initializeTestFramework();
    
    /**
     * Start the complete systematic testing procedure
     * @param thermalMass Expected thermal mass category (optional, auto-detected if not specified)
     * @return true if test sequence started successfully
     */
    bool startSystematicTests(ThermalMassCategory thermalMass = ThermalMassCategory::MEDIUM_DUAL_BOILER);
    
    /**
     * Update the test state machine (call from main loop)
     * @param currentTemp Current temperature reading
     * @param currentPressure Current pressure reading (bar)
     * @return true if tests are still running
     */
    bool updateTests(float currentTemp, float currentPressure);
    
    /**
     * Get current test phase
     */
    TestPhase getCurrentPhase() const { return currentPhase; }
    
    /**
     * Check if tests are complete
     */
    bool isTestComplete() const { return currentPhase == TestPhase::COMPLETE; }
    
    /**
     * Get comprehensive test results
     */
    const SystemTestResults& getTestResults() const { return testResults; }
    
    /**
     * Abort current test sequence
     */
    void abortTests();
    
    /**
     * Individual test functions
     */
    bool runColdStartTest();
    bool runSteamTransitionTest();
    bool runStabilityTest(uint32_t durationMs = STABILITY_TEST_DURATION_MS);
    bool runPressureSensorTest();
    bool runEmergencyShutdownTest();
    
    /**
     * PID tuning and optimization functions
     */
    void autoTunePIDForThermalMass(ThermalMassCategory thermalMass);
    void optimizePIDParameters();
    ThermalMassCategory detectThermalMass(float heatupRate, uint32_t thermalResponseTime);
    
    /**
     * Documentation and reporting
     */
    void generateTestReport();
    void savePIDParametersForMachine(ThermalMassCategory thermalMass);
    void loadOptimalPIDParameters(ThermalMassCategory thermalMass);
    
    /**
     * Safety monitoring during tests
     */
    bool checkTestSafety(float currentTemp, bool heaterState);
    void emergencyShutdown(const char* reason);
    
private:
    TestPhase currentPhase;
    SystemTestResults testResults;
    uint32_t testStartTime;
    uint32_t phaseStartTime;
    ThermalMassCategory targetThermalMass;
    
    // Test state tracking
    float initialTemp;
    float maxTempReached;
    float minTempReached;
    uint32_t stabilityViolations;
    bool testAborted;
    
    // Pressure test tracking
    float pressureTestMin;
    float pressureTestMax;
    uint32_t pressureReadingCount;
    
    // Emergency shutdown test
    bool emergencyTestActive;
    uint32_t emergencyTriggerTime;
    
    // PID tuning state
    PIDController* testPIDController;
    uint32_t tuningIterations;
    
    // Phase-specific test methods
    bool updateColdStartTest(float currentTemp);
    bool updateSteamTransitionTest(float currentTemp);
    bool updateStabilityTest(float currentTemp);
    bool updatePressureTest(float currentPressure);
    bool updateEmergencyTest(float currentTemp);
    
    // Analysis and optimization
    void analyzeHeatupCharacteristics(float currentTemp, uint32_t elapsedTime);
    void calculateStabilityMetrics(float currentTemp);
    bool validatePressureRange(float pressure);
    
    // Reporting helpers
    void logTestProgress(const char* phase, float progress);
    void logTestResults();
    void documentPIDParameters();
    
    // Safety helpers
    bool isTemperatureSafe(float temp);
    bool isTestTimeout();
    void resetTestState();
};

/**
 * Global test framework instance
 */
extern SystemTestFramework systemTest;

/**
 * Convenience functions for easy test execution
 */
void initSystemTesting();
bool runFullSystemTest();
void runQuickValidationTest();
void testPressureSensorCalibration();
void validateEmergencyShutdown();

/**
 * PID parameter sets for different machine types
 */
extern const PIDTuningSet SMALL_BOILER_PID;
extern const PIDTuningSet MEDIUM_BOILER_PID;
extern const PIDTuningSet LARGE_BOILER_PID;

#endif // SYSTEM_TEST_FRAMEWORK_H
