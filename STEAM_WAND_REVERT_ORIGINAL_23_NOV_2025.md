# Steam Wand Logic - Reverted to Original - November 23, 2025

## Request
User requested to revert steam wand logic to the "original project" implementation because "steam was working fine there" and "all my changes wasnt mean to change the steam process".

## Action Taken
Restored `steamCtrl` function in `src/functional/just_do_coffee.cpp` to match the implementation found in historical commits (e.g. from 2023).

### Key Characteristics of Original Logic:
1. **Strict Cutoff**: If `Temperature > Setpoint` OR `Pressure > Threshold`, **ALL** components (Heater, Pump, Valves) are turned OFF.
2. **Heater Control**: Heater is ON only if `Temperature < Setpoint`. No hysteresis above setpoint.
3. **Pump Control**: 
   - Enabled only if `Pressure < activeSteamPressure_` (typically 2.0 bar).
   - Pump power is set to **`3`** (low trickle), not 15 or 45.
4. **No Special PWM Handling**: The logic applies universally, regardless of `USE_HARDWARE_TIMER_PWM` (simplified structure).

## Logic Details
```cpp
if (currentState.smoothedPressure > steamThreshold_ || sensorTemperature > steamTempSetPoint) {
  // Safety Cutoff
  setBoilerOff();
  setSteamBoilerRelayOff();
  setSteamValveRelayOff();
  setPumpOff();
} else {
  // Active Steam Mode
  if (sensorTemperature < steamTempSetPoint) {
    setBoilerOn();
  } else {
    setBoilerOff();
  }
  setSteamValveRelayOn();
  setSteamBoilerRelayOn();
  
  #ifndef DREAM_STEAM_DISABLED
    if (currentState.smoothedPressure < activeSteamPressure_) {
      setPumpToRawValue(3); // Original low flow value
    } else {
      setPumpOff();
    }
  #endif
}
```

## Verification
This logic matches the standard Gaggiuino implementation before recent modifications. If the hardware setup is standard (or compatible with standard logic), this should restore the previous "working fine" behavior.

## Firmware
The firmware file will be built with this restored logic.

