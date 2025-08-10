/* Maintenance Menu for Temperature Control Diagnostics */
#ifndef MAINTENANCE_MENU_H
#define MAINTENANCE_MENU_H

#include <Arduino.h>
#include "../log.h"
#include "../eeprom_data/eeprom_data.h"
#include "../../lib/Common/sensors_state.h"

/**
 * Maintenance Menu System
 * Provides access to diagnostic and testing functions
 */
class MaintenanceMenu {
public:
    MaintenanceMenu();
    
    /**
     * Initialize maintenance menu system
     */
    void init();
    
    /**
     * Check if maintenance menu should be activated
     * @param currentState Current sensor state
     * @return true if menu should be shown
     */
    bool shouldShowMenu(const SensorState& currentState);
    
    /**
     * Display maintenance menu options
     */
    void displayMenu();
    
    /**
     * Process user input for maintenance menu
     * @param input User input character
     * @param currentState Current sensor state
     * @param runningCfg Current configuration
     * @return true if input was handled
     */
    bool processInput(char input, const SensorState& currentState, const eepromValues_t& runningCfg);
    
    /**
     * Update maintenance menu (call periodically)
     * @param currentState Current sensor state
     * @param runningCfg Current configuration
     */
    void update(const SensorState& currentState, const eepromValues_t& runningCfg);
    
private:
    enum MenuState {
        MENU_MAIN,
        MENU_TEMP_DIAGNOSTICS,
        MENU_PID_DEBUG,
        MENU_STEP_TEST,
        MENU_SAFETY_TEST,
        MENU_EXIT
    };
    
    MenuState currentMenuState;
    bool menuActive;
    uint32_t menuActivatedTime;
    uint32_t lastMenuUpdate;
    
    // Diagnostic states
    bool pidDebugActive;
    bool diagnosticLoggingActive;
    bool stepTestActive;
    float preStepTestSetpoint;
    
    // Menu display functions
    void displayMainMenu();
    void displayTempDiagnosticsMenu();
    void displayPIDDebugMenu();
    void displayStepTestMenu();
    void displaySafetyTestMenu();
    
    // Menu action functions
    void activatePIDDebugMode();
    void deactivatePIDDebugMode();
    void startDiagnosticLogging();
    void stopDiagnosticLogging();
    void startStepResponseTest(float currentSetpoint);
    void stopStepResponseTest();
    void displayStepTestResults();
    void runSafetyTests(const SensorState& currentState);
    void displaySystemStatus(const SensorState& currentState);
    
    // Input handling
    bool handleMainMenuInput(char input, const SensorState& currentState, const eepromValues_t& runningCfg);
    bool handleTempDiagnosticsInput(char input, const SensorState& currentState, const eepromValues_t& runningCfg);
    bool handlePIDDebugInput(char input);
    bool handleStepTestInput(char input, const SensorState& currentState);
    bool handleSafetyTestInput(char input, const SensorState& currentState);
    
    // Utility functions
    void printSeparator();
    void printMenuHeader(const char* title);
    bool checkMenuActivationSequence();
};

// Global instance
extern MaintenanceMenu maintenanceMenu;

// Convenience functions
void initMaintenanceMenu();
void updateMaintenanceMenu(const SensorState& currentState, const eepromValues_t& runningCfg);
bool processMaintenanceMenuInput(char input, const SensorState& currentState, const eepromValues_t& runningCfg);

#endif // MAINTENANCE_MENU_H
