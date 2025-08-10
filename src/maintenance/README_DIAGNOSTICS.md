# Temperature Control Diagnostics

## Overview

This implementation adds comprehensive diagnostic features for testing and tuning the PID temperature control system:

### Features Implemented

1. **Temperature Control Debug Mode** - Logs detailed PID terms via serial
2. **Temperature Step Response Test** - Automated testing for PID tuning analysis  
3. **Overshoot Detection & Auto-Detuning** - Automatic PID parameter adjustment
4. **Comprehensive Logging** - Temperature, PID output, and heater state logging
5. **Safety Limits** - Maximum temperature (110°C) and continuous heating time (30s) protection

## Usage

### Accessing the Maintenance Menu

1. Ensure the machine is idle (not brewing or steaming)
2. Send `MAINT` via serial console
3. The maintenance menu will appear with options:
   - 1. Temperature Diagnostics
   - 2. PID Debug Mode  
   - 3. Step Response Test
   - 4. Safety Tests
   - 5. System Status

### PID Debug Mode

Enables detailed logging of PID controller terms:
- **Setpoint** - Target temperature
- **Input** - Current temperature
- **Output** - PID controller output (0-100%)
- **Error** - Difference between setpoint and input
- **P, I, D terms** - Individual PID components
- **Overshoot detection** - Automatic detection and correction

**Usage:**
1. Access Maintenance Menu → PID Debug Mode
2. Enable debug mode  
3. Monitor serial output for PID terms
4. Use data for manual PID tuning

### Step Response Test

Automated test that measures system response to temperature step changes:

**Measurements:**
- **Rise Time** - Time to reach 63% of final value
- **Settling Time** - Time to stay within 2% of target
- **Overshoot** - Maximum percentage overshoot
- **Steady-State Error** - Final error from target

**Usage:**
1. Access Maintenance Menu → Step Response Test
2. Choose step size:
   - Option 1: +10°C step (recommended for full system analysis)
   - Option 2: -5°C step (for cooling response analysis)
3. Test runs for 60 seconds automatically
4. View results with PID tuning recommendations

### Temperature Safety System

Automatic safety monitoring with limits:
- **Maximum Temperature:** 110°C (emergency shutdown)
- **Continuous Heating Time:** 30 seconds maximum
- **Temperature Rate Limiting:** Prevents sudden temperature changes

**Safety Features:**
- Automatic heater shutdown on limit violation
- Logging of safety events
- Statistics tracking (max temperature recorded)
- Manual safety test and status display

### Diagnostic Data Logging

Structured CSV logging for analysis:

**Format:** `TEMP_DATA,timestamp,setpoint,actual,output,heater,P,I,D`

**Example Output:**
```
TEMP_DATA,123456,93.5,92.8,45.2,1,2.34,-0.12,0.89
TEMP_DATA,124456,93.5,93.1,38.7,1,1.67,-0.08,0.76
```

**Usage:**
1. Enable diagnostic logging via maintenance menu
2. Copy serial output to CSV file
3. Import into Excel/spreadsheet for analysis
4. Use for PID tuning and performance optimization

## Integration Points

### Main Loop Integration

Add these calls to your main temperature control loop:

```cpp
#include "peripherals/temperature_safety.h"
#include "maintenance/maintenance_menu.h"

void setup() {
    // ... existing setup ...
    initTemperatureSafety();
    initMaintenanceMenu();
}

void loop() {
    // ... existing code ...
    
    // Check safety limits before heater control
    bool isSafe = checkTemperatureSafety(currentTemp, heaterState);
    if (!isSafe) {
        // Safety violation - heater already shut down
        return;
    }
    
    // Update maintenance menu
    updateMaintenanceMenu(currentState, runningCfg);
    
    // Log diagnostics if enabled
    logTemperatureDiagnostics(setpoint, currentTemp, pidOutput, 
                             heaterState, pidP, pidI, pidD);
}

// Serial input handler
void handleSerialInput() {
    if (Serial.available()) {
        char input = Serial.read();
        bool handled = processMaintenanceMenuInput(input, currentState, runningCfg);
        if (!handled) {
            // Handle other serial commands
        }
    }
}
```

### PID Tuning Recommendations

Based on step test results:

**High Overshoot (>10%):**
- Reduce Kp (proportional gain)
- Reduce Kd (derivative gain)

**Slow Response (rise time >30s):**
- Increase Kp
- Check for mechanical issues

**Steady-State Error (>1°C):**
- Increase Ki (integral gain)
- Check temperature sensor calibration

**Oscillation:**
- Reduce Kp and Kd
- Increase derivative filtering
- Check for noise in temperature readings

## File Structure

```
src/
├── peripherals/
│   ├── temperature_safety.h/.cpp     # Safety monitoring and step tests
│   ├── pid_controller.h/.cpp         # Enhanced PID with diagnostics  
│   └── heater_control.h/.cpp         # Integration functions
└── maintenance/
    ├── maintenance_menu.h/.cpp       # Interactive maintenance menu
    └── README_DIAGNOSTICS.md         # This documentation
```

## Safety Warnings

⚠️ **IMPORTANT SAFETY NOTES:**

1. **Temperature Limits:** The 110°C limit is set for safety. Do not disable without proper thermal protection.

2. **Step Tests:** Only run step tests when machine is stable and attended. Tests change temperature setpoints.

3. **Continuous Heating:** The 30-second limit prevents overheating. Monitor during diagnostic tests.

4. **Manual Intervention:** Always be prepared to manually shut down the system during testing.

5. **Calibration:** Verify temperature sensor calibration before relying on safety limits.

## Troubleshooting

**Menu Won't Activate:**
- Check that machine is idle (not brewing/steaming)
- Verify serial connection
- Ensure you send exactly "MAINT"

**Step Test Not Starting:**
- Check current temperature is stable
- Verify PID controller is initialized
- Ensure no other tests are running

**No Debug Output:**
- Verify debug mode is enabled
- Check serial baud rate settings
- Ensure LOG_INFO is properly configured

**Safety Violations:**
- Check temperature sensor connections
- Verify heater control wiring
- Review system thermal design

## Data Analysis

### Excel Analysis Template

1. Import CSV data with comma delimiter
2. Create charts for:
   - Temperature vs Time
   - PID Output vs Time  
   - P, I, D terms vs Time
3. Calculate performance metrics:
   - Average error
   - Standard deviation
   - Settling time
   - Overshoot percentage

### Performance Optimization

Monitor these key metrics:
- **Temperature Stability:** ±0.5°C target
- **Response Time:** <30 seconds to setpoint
- **Overshoot:** <5% maximum  
- **Energy Efficiency:** Minimize average PID output

This diagnostic system provides comprehensive tools for optimizing temperature control performance while maintaining safety.
