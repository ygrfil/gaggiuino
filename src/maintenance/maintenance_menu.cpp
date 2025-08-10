/* Maintenance Menu Implementation */
#include "maintenance_menu.h"
#include "../peripherals/temperature_safety.h"
#include "../peripherals/heater_control.h"
#include "../peripherals/pid_controller.h"

// Global instance
MaintenanceMenu maintenanceMenu;

MaintenanceMenu::MaintenanceMenu()
    : currentMenuState(MENU_MAIN)
    , menuActive(false)
    , menuActivatedTime(0)
    , lastMenuUpdate(0)
    , pidDebugActive(false)
    , diagnosticLoggingActive(false)
    , stepTestActive(false)
    , preStepTestSetpoint(0.0f)
{
}

void MaintenanceMenu::init() {
    LOG_INFO("Maintenance Menu initialized");
    LOG_INFO("To access maintenance menu: Send 'MAINT' via serial when machine is idle");
}

bool MaintenanceMenu::shouldShowMenu(const SensorState& currentState) {
    // Menu can be activated when machine is not brewing and not in steam mode
    return !currentState.brewSwitchState && !currentState.steamSwitchState;
}

void MaintenanceMenu::displayMenu() {
    switch (currentMenuState) {
        case MENU_MAIN:
            displayMainMenu();
            break;
        case MENU_TEMP_DIAGNOSTICS:
            displayTempDiagnosticsMenu();
            break;
        case MENU_PID_DEBUG:
            displayPIDDebugMenu();
            break;
        case MENU_STEP_TEST:
            displayStepTestMenu();
            break;
        case MENU_SAFETY_TEST:
            displaySafetyTestMenu();
            break;
        default:
            currentMenuState = MENU_MAIN;
            break;
    }
}

bool MaintenanceMenu::processInput(char input, const SensorState& currentState, const eepromValues_t& runningCfg) {
    if (!menuActive) {
        // Check for menu activation
        static String activationBuffer = "";
        activationBuffer += input;
        if (activationBuffer.length() > 5) {
            activationBuffer = activationBuffer.substring(1);
        }
        
        if (activationBuffer.equals("MAINT") && shouldShowMenu(currentState)) {
            menuActive = true;
            menuActivatedTime = millis();
            currentMenuState = MENU_MAIN;
            LOG_INFO("Maintenance Menu activated");
            displayMenu();
            return true;
        }
        return false;
    }
    
    // Process menu input based on current state
    bool handled = false;
    switch (currentMenuState) {
        case MENU_MAIN:
            handled = handleMainMenuInput(input, currentState, runningCfg);
            break;
        case MENU_TEMP_DIAGNOSTICS:
            handled = handleTempDiagnosticsInput(input, currentState, runningCfg);
            break;
        case MENU_PID_DEBUG:
            handled = handlePIDDebugInput(input);
            break;
        case MENU_STEP_TEST:
            handled = handleStepTestInput(input, currentState);
            break;
        case MENU_SAFETY_TEST:
            handled = handleSafetyTestInput(input, currentState);
            break;
        default:
            break;
    }
    
    // Exit menu if 'x' or 'X' is pressed
    if (input == 'x' || input == 'X') {
        menuActive = false;
        LOG_INFO("Maintenance Menu deactivated");
        return true;
    }
    
    return handled;
}

void MaintenanceMenu::update(const SensorState& currentState, const eepromValues_t& runningCfg) {
    if (!menuActive) {
        return;
    }
    
    uint32_t currentTime = millis();
    
    // Auto-exit menu after 5 minutes of inactivity
    if (currentTime - menuActivatedTime > 300000) {
        menuActive = false;
        LOG_INFO("Maintenance Menu auto-deactivated due to inactivity");
        return;
    }
    
    // Update step test if running
    if (stepTestActive) {
        updateTemperatureStepTest(currentState.temperature);
        
        // Check if step test completed
        if (!stepTest.isStepTestRunning()) {
            stepTestActive = false;
            LOG_INFO("Step test completed");
            displayStepTestResults();
        }
    }
    
    // Periodic status updates (every 10 seconds)
    if (currentTime - lastMenuUpdate > 10000) {
        if (pidDebugActive || diagnosticLoggingActive || stepTestActive) {
            LOG_INFO("Diagnostics Status - PID Debug: %s, Logging: %s, Step Test: %s",
                     pidDebugActive ? "ON" : "OFF",
                     diagnosticLoggingActive ? "ON" : "OFF", 
                     stepTestActive ? "RUNNING" : "OFF");
        }
        lastMenuUpdate = currentTime;
    }
}

// ============================================================================
// Menu Display Functions
// ============================================================================

void MaintenanceMenu::displayMainMenu() {
    printMenuHeader("MAINTENANCE MENU");
    LOG_INFO("1. Temperature Diagnostics");
    LOG_INFO("2. PID Debug Mode");
    LOG_INFO("3. Step Response Test");
    LOG_INFO("4. Safety Tests");
    LOG_INFO("5. System Status");
    LOG_INFO("X. Exit Menu");
    printSeparator();
    LOG_INFO("Select option (1-5, X):");
}

void MaintenanceMenu::displayTempDiagnosticsMenu() {
    printMenuHeader("TEMPERATURE DIAGNOSTICS");
    LOG_INFO("Current Status:");
    LOG_INFO("  PID Debug: %s", pidDebugActive ? "ACTIVE" : "INACTIVE");
    LOG_INFO("  Data Logging: %s", diagnosticLoggingActive ? "ACTIVE" : "INACTIVE");
    LOG_INFO("  Step Test: %s", stepTestActive ? "RUNNING" : "IDLE");
    LOG_INFO("");
    LOG_INFO("1. Toggle PID Debug Mode");
    LOG_INFO("2. Toggle Diagnostic Logging");
    LOG_INFO("3. Start Step Response Test");
    LOG_INFO("4. View Step Test Results");
    LOG_INFO("B. Back to Main Menu");
    LOG_INFO("X. Exit Menu");
    printSeparator();
    LOG_INFO("Select option:");
}

void MaintenanceMenu::displayPIDDebugMenu() {
    printMenuHeader("PID DEBUG MODE");
    LOG_INFO("PID Debug is currently: %s", pidDebugActive ? "ACTIVE" : "INACTIVE");
    LOG_INFO("");
    LOG_INFO("Debug mode logs PID terms via serial:");
    LOG_INFO("- Setpoint, Input, Output, Error");
    LOG_INFO("- Proportional, Integral, Derivative terms");
    LOG_INFO("- Overshoot detection and auto-detuning");
    LOG_INFO("");
    LOG_INFO("1. Enable PID Debug");
    LOG_INFO("2. Disable PID Debug");
    LOG_INFO("B. Back");
    LOG_INFO("X. Exit Menu");
    printSeparator();
}

void MaintenanceMenu::displayStepTestMenu() {
    printMenuHeader("STEP RESPONSE TEST");
    LOG_INFO("Step test status: %s", stepTestActive ? "RUNNING" : "IDLE");
    LOG_INFO("");
    LOG_INFO("Step response test measures:");
    LOG_INFO("- Rise time (time to 63%% of final value)");
    LOG_INFO("- Settling time (time to stay within 2%%)");
    LOG_INFO("- Overshoot percentage");
    LOG_INFO("- Steady-state error");
    LOG_INFO("");
    LOG_INFO("1. Start +10°C Step Test");
    LOG_INFO("2. Start -5°C Step Test");
    LOG_INFO("3. Stop Current Test");
    LOG_INFO("4. View Last Results");
    LOG_INFO("B. Back");
    LOG_INFO("X. Exit Menu");
    printSeparator();
}

void MaintenanceMenu::displaySafetyTestMenu() {
    printMenuHeader("SAFETY TESTS");
    LOG_INFO("Safety system status:");
    LOG_INFO("  Max Temperature Limit: %.1f°C", static_cast<double>(MAX_SAFE_TEMPERATURE));
    LOG_INFO("  Max Continuous Heating: %lu seconds", MAX_CONTINUOUS_HEATING_TIME / 1000);
    LOG_INFO("  Current Max Recorded: %.1f°C", static_cast<double>(temperatureSafety.getMaxTemperatureRecorded()));
    LOG_INFO("");
    LOG_INFO("1. View Safety Status");
    LOG_INFO("2. Reset Safety Statistics");
    LOG_INFO("3. Test Temperature Limit Warning");
    LOG_INFO("B. Back");
    LOG_INFO("X. Exit Menu");
    printSeparator();
}

// ============================================================================
// Menu Action Functions  
// ============================================================================

void MaintenanceMenu::activatePIDDebugMode() {
    enablePIDDebugMode(true);
    pidDebugActive = true;
    LOG_INFO("PID Debug mode ACTIVATED - Watch serial output for PID terms");
}

void MaintenanceMenu::deactivatePIDDebugMode() {
    enablePIDDebugMode(false);
    pidDebugActive = false;
    LOG_INFO("PID Debug mode DEACTIVATED");
}

void MaintenanceMenu::startDiagnosticLogging() {
    enableTemperatureDiagnosticLogging(true);
    diagnosticLoggingActive = true;
    LOG_INFO("Temperature diagnostic logging STARTED");
    LOG_INFO("CSV format: TEMP_DATA,timestamp,setpoint,actual,output,heater,P,I,D");
}

void MaintenanceMenu::stopDiagnosticLogging() {
    enableTemperatureDiagnosticLogging(false);
    diagnosticLoggingActive = false;
    LOG_INFO("Temperature diagnostic logging STOPPED");
}

void MaintenanceMenu::startStepResponseTest(float currentSetpoint) {
    preStepTestSetpoint = currentSetpoint;
    startTemperatureStepTest(currentSetpoint);
    stepTestActive = true;
    LOG_INFO("Step response test STARTED from %.1f°C", static_cast<double>(currentSetpoint));
}

void MaintenanceMenu::stopStepResponseTest() {
    stepTest.stopStepTest();
    stepTestActive = false;
    LOG_INFO("Step response test STOPPED");
}

void MaintenanceMenu::displayStepTestResults() {
    uint32_t riseTime, settleTime;
    float overshoot, steadyStateError;
    
    if (stepTest.getStepTestResults(riseTime, settleTime, overshoot, steadyStateError)) {
        printMenuHeader("STEP TEST RESULTS");
        LOG_INFO("Rise Time (63%%): %lu ms (%.1f seconds)", riseTime, riseTime / 1000.0f);
        LOG_INFO("Settle Time: %lu ms (%.1f seconds)", settleTime, settleTime / 1000.0f);
        LOG_INFO("Overshoot: %.2f%%", static_cast<double>(overshoot));
        LOG_INFO("Steady-State Error: %.2f°C", static_cast<double>(steadyStateError));
        printSeparator();
        
        // Provide PID tuning recommendations
        LOG_INFO("PID TUNING RECOMMENDATIONS:");
        if (overshoot > 10.0f) {
            LOG_INFO("- Overshoot too high: Reduce Kp and/or Kd");
        } else if (overshoot < 2.0f) {
            LOG_INFO("- System may be under-damped: Consider increasing Kp slightly");
        }
        
        if (abs(steadyStateError) > 1.0f) {
            LOG_INFO("- Steady-state error high: Increase Ki");
        }
        
        if (riseTime > 30000) {  // > 30 seconds
            LOG_INFO("- Rise time slow: Increase Kp");
        }
    } else {
        LOG_INFO("No step test results available. Run a step test first.");
    }
}

void MaintenanceMenu::runSafetyTests(const SensorState& currentState) {
    printMenuHeader("SAFETY TEST RESULTS");
    
    // Check current temperature against limit
    float tempMargin = MAX_SAFE_TEMPERATURE - currentState.temperature;
    LOG_INFO("Current Temperature: %.1f°C", static_cast<double>(currentState.temperature));
    LOG_INFO("Safety Limit: %.1f°C", static_cast<double>(MAX_SAFE_TEMPERATURE));
    LOG_INFO("Safety Margin: %.1f°C", static_cast<double>(tempMargin));
    
    if (tempMargin < 10.0f) {
        LOG_WARN("WARNING: Temperature is within 10°C of safety limit!");
    }
    
    // Check continuous heating time
    uint32_t remainingTime = temperatureSafety.getContinuousHeatingTimeRemaining();
    LOG_INFO("Continuous Heating Time Remaining: %lu seconds", remainingTime / 1000);
    
    if (remainingTime < 10000) {  // Less than 10 seconds
        LOG_WARN("WARNING: Continuous heating time nearly expired!");
    }
    
    LOG_INFO("Max Recorded Temperature: %.1f°C", static_cast<double>(temperatureSafety.getMaxTemperatureRecorded()));
    printSeparator();
}

void MaintenanceMenu::displaySystemStatus(const SensorState& currentState) {
    printMenuHeader("SYSTEM STATUS");
    LOG_INFO("Temperature: %.1f°C", static_cast<double>(currentState.temperature));
    LOG_INFO("Pressure: %.1f bar", static_cast<double>(currentState.pressure));
    LOG_INFO("Water Level: %d%%", currentState.waterLvl);
    LOG_INFO("Brew Switch: %s", currentState.brewSwitchState ? "ACTIVE" : "INACTIVE");
    LOG_INFO("Steam Switch: %s", currentState.steamSwitchState ? "ACTIVE" : "INACTIVE");
    LOG_INFO("Scales Present: %s", currentState.scalesPresent ? "YES" : "NO");
    
    if (currentState.scalesPresent) {
        LOG_INFO("Current Weight: %.1fg", static_cast<double>(currentState.weight));
    }
    
    // PID status
    float kp, ki, kd;
    getPIDTunings(kp, ki, kd);
    LOG_INFO("PID Tunings - Kp: %.2f, Ki: %.2f, Kd: %.2f", 
             static_cast<double>(kp), static_cast<double>(ki), static_cast<double>(kd));
    LOG_INFO("PID Output: %.1f%%", static_cast<double>(getPIDOutput()));
    LOG_INFO("PID Error: %.2f°C", static_cast<double>(getPIDError()));
    
    printSeparator();
}

// ============================================================================
// Input Handling Functions
// ============================================================================

bool MaintenanceMenu::handleMainMenuInput(char input, const SensorState& currentState, const eepromValues_t& runningCfg) {
    switch (input) {
        case '1':
            currentMenuState = MENU_TEMP_DIAGNOSTICS;
            displayMenu();
            return true;
        case '2':
            currentMenuState = MENU_PID_DEBUG;
            displayMenu();
            return true;
        case '3':
            currentMenuState = MENU_STEP_TEST;
            displayMenu();
            return true;
        case '4':
            currentMenuState = MENU_SAFETY_TEST;
            displayMenu();
            return true;
        case '5':
            displaySystemStatus(currentState);
            return true;
        default:
            return false;
    }
}

bool MaintenanceMenu::handleTempDiagnosticsInput(char input, const SensorState& currentState, const eepromValues_t& runningCfg) {
    switch (input) {
        case '1':
            if (pidDebugActive) {
                deactivatePIDDebugMode();
            } else {
                activatePIDDebugMode();
            }
            displayMenu();
            return true;
        case '2':
            if (diagnosticLoggingActive) {
                stopDiagnosticLogging();
            } else {
                startDiagnosticLogging();
            }
            displayMenu();
            return true;
        case '3':
            if (!stepTestActive) {
                startStepResponseTest(currentState.temperature);
            } else {
                LOG_INFO("Step test already running. Stop current test first.");
            }
            return true;
        case '4':
            displayStepTestResults();
            return true;
        case 'b':
        case 'B':
            currentMenuState = MENU_MAIN;
            displayMenu();
            return true;
        default:
            return false;
    }
}

bool MaintenanceMenu::handlePIDDebugInput(char input) {
    switch (input) {
        case '1':
            activatePIDDebugMode();
            displayMenu();
            return true;
        case '2':
            deactivatePIDDebugMode();
            displayMenu();
            return true;
        case 'b':
        case 'B':
            currentMenuState = MENU_MAIN;
            displayMenu();
            return true;
        default:
            return false;
    }
}

bool MaintenanceMenu::handleStepTestInput(char input, const SensorState& currentState) {
    switch (input) {
        case '1':  // +10°C step
            if (!stepTestActive) {
                startStepResponseTest(currentState.temperature);
            } else {
                LOG_INFO("Step test already running");
            }
            return true;
        case '2':  // -5°C step  
            if (!stepTestActive) {
                stepTest.startStepTest(currentState.temperature, -5.0f);
                stepTestActive = true;
                LOG_INFO("Negative step test started");
            } else {
                LOG_INFO("Step test already running");
            }
            return true;
        case '3':  // Stop test
            if (stepTestActive) {
                stopStepResponseTest();
            }
            displayMenu();
            return true;
        case '4':  // View results
            displayStepTestResults();
            return true;
        case 'b':
        case 'B':
            currentMenuState = MENU_MAIN;
            displayMenu();
            return true;
        default:
            return false;
    }
}

bool MaintenanceMenu::handleSafetyTestInput(char input, const SensorState& currentState) {
    switch (input) {
        case '1':
            runSafetyTests(currentState);
            return true;
        case '2':
            temperatureSafety.resetTemperatureStats();
            LOG_INFO("Safety statistics reset");
            displayMenu();
            return true;
        case '3':
            LOG_WARN("SAFETY TEST: Simulating temperature limit approach");
            LOG_WARN("This is a test - no actual danger");
            return true;
        case 'b':
        case 'B':
            currentMenuState = MENU_MAIN;
            displayMenu();
            return true;
        default:
            return false;
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

void MaintenanceMenu::printSeparator() {
    LOG_INFO("================================================");
}

void MaintenanceMenu::printMenuHeader(const char* title) {
    printSeparator();
    LOG_INFO("           %s", title);
    printSeparator();
}

// ============================================================================
// Global Convenience Functions
// ============================================================================

void initMaintenanceMenu() {
    maintenanceMenu.init();
}

void updateMaintenanceMenu(const SensorState& currentState, const eepromValues_t& runningCfg) {
    maintenanceMenu.update(currentState, runningCfg);
}

bool processMaintenanceMenuInput(char input, const SensorState& currentState, const eepromValues_t& runningCfg) {
    return maintenanceMenu.processInput(input, currentState, runningCfg);
}
