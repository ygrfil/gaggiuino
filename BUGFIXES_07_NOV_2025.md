# Bug Fixes - November 7, 2025

## Summary
This document outlines bugs found and fixed during code review of the Gaggiuino firmware.

## Build Information
- **Firmware Version:** `1dffcfbc`
- **Build Date:** November 7, 2025
- **Environment:** `all-pcb-stlink` (Single Board PCB)
- **Binary:** `gaggiuino-BUGFIXES-07-Nov-2025.bin`
- **Flash Usage:** 26.4% (138,320 / 524,288 bytes)
- **RAM Usage:** 22.3% (29,292 / 131,072 bytes)
- **Build Status:** ✅ Success

---

## Bugs Fixed

### 1. **CRITICAL: Logic Error in rampPhaseIndex Check**
**File:** `src/gaggiuino.ino` (lines 666, 669, 684, 687, 719)  
**Severity:** Medium  
**Issue:** `rampPhaseIndex` initialized to `-1`, but code checked `rampPhaseIndex > 0`, which incorrectly rejected valid index `0`.

**Problem:**
```cpp
int rampPhaseIndex = -1;
// ... later ...
rampPhaseIndex = rampPhaseIndex > 0 ? rampPhaseIndex : profile.phaseCount() - 1;
```

If `rampPhaseIndex` was set to `0` (a valid first phase index), the check `rampPhaseIndex > 0` would be false, causing it to be overwritten incorrectly.

**Fix:**
Changed all instances from `rampPhaseIndex > 0` to `rampPhaseIndex >= 0` to correctly handle index 0.

**Impact:** Prevents incorrect ramp phase insertion when first phase is at index 0.

---

### 2. **CRITICAL: Division by Zero in Water Temperature Calculation**
**File:** `src/gaggiuino.ino` (lines 405-410)  
**Severity:** Medium  
**Issue:** Division by `brewTempSetPoint` without checking for zero or very small values.

**Problem:**
```cpp
currentState.waterTemperature = (currentState.temperature > (float)ACTIVE_PROFILE(runningCfg).setpoint && currentState.brewSwitchState)
  ? currentState.temperature / (float)brewTempSetPoint + (float)ACTIVE_PROFILE(runningCfg).setpoint
  : currentState.temperature;
```

If `brewTempSetPoint` is 0 or very small (due to invalid configuration), this causes division by zero or invalid results.

**Fix:**
Added safety check `brewTempSetPoint > 0.1f` before division:
```cpp
if (currentState.temperature > (float)ACTIVE_PROFILE(runningCfg).setpoint && currentState.brewSwitchState && brewTempSetPoint > 0.1f) {
  currentState.waterTemperature = currentState.temperature / (float)brewTempSetPoint + (float)ACTIVE_PROFILE(runningCfg).setpoint;
} else {
  currentState.waterTemperature = currentState.temperature;
}
```

**Impact:** Prevents crashes and invalid temperature calculations from division by zero.

---

### 3. **CRITICAL: Division by Zero in Flow Calculation**
**File:** `src/peripherals/pump.cpp` (lines 135-139)  
**Severity:** Medium  
**Issue:** `getClicksPerSecondForFlow` divides by `flowPerClick` without checking for zero.

**Problem:**
```cpp
float getClicksPerSecondForFlow(const float flow, const float pressure) {
  if (flow == 0.f) return 0;
  float flowPerClick = getPumpFlowPerClick(pressure);
  float cps = flow / flowPerClick;  // Potential division by zero
  return fminf(cps, (float)maxPumpClicksPerSecond);
}
```

If `getPumpFlowPerClick` returns 0 or very small value (e.g., at extreme pressures), this causes division by zero or extremely large values.

**Fix:**
Added safety check for `flowPerClick <= 0.0001f` and return maximum pump capacity as fallback:
```cpp
float flowPerClick = getPumpFlowPerClick(pressure);
if (flowPerClick <= 0.0001f) {
  // If flow per click is too small or zero, return maximum pump capacity as fallback
  return (float)maxPumpClicksPerSecond;
}
float cps = flow / flowPerClick;
```

**Impact:** Prevents crashes and erratic pump behavior when flow calculations fail.

---

### 4. **Redundant Function Call in hotWaterMode**
**File:** `src/functional/just_do_coffee.cpp` (lines 224-233)  
**Severity:** Low  
**Issue:** `setBoilerOn()` called twice consecutively.

**Problem:**
```cpp
void hotWaterMode(const SensorState &currentState) {
  closeValve();
  setPumpToRawValue(80);
  setBoilerOn();  // First call
  if (currentState.temperature < MAX_WATER_TEMP) setBoilerOn();  // Redundant second call
  else setBoilerOff();
}
```

The second call is redundant if the condition is true.

**Fix:**
Removed redundant call and restructured logic:
```cpp
void hotWaterMode(const SensorState &currentState) {
  closeValve();
  setPumpToRawValue(80);
  if (currentState.temperature < MAX_WATER_TEMP) {
    setBoilerOn();
  } else {
    setBoilerOff();
  }
}
```

**Impact:** Cleaner code, eliminates redundant function call.

---

### 5. **CRITICAL: Array Underflow in Ramp Phase Insertion**
**File:** `src/gaggiuino.ino` (lines 717-724, 732-733)  
**Severity:** Medium  
**Issue:** `profile.phaseCount() - 1` can underflow if profile is empty.

**Problem:**
```cpp
rampPhaseIndex = rampPhaseIndex > 0 ? rampPhaseIndex : profile.phaseCount() - 1;
insertRampPhaseIfNeeded(rampPhaseIndex);
```

If `profile.phaseCount()` is 0, `profile.phaseCount() - 1` wraps to a large unsigned value (underflow), causing out-of-bounds access.

**Fix:**
1. Added check before accessing phases:
```cpp
if (profile.phaseCount() > 0) {
  rampPhaseIndex = rampPhaseIndex >= 0 ? rampPhaseIndex : profile.phaseCount() - 1;
  insertRampPhaseIfNeeded(rampPhaseIndex);
}
```

2. Added bounds check in `insertRampPhaseIfNeeded()`:
```cpp
if (rampPhaseIndex <= 0 || rampPhaseIndex >= profile.phaseCount() || rampTime <= 0 || rampCurve == TransitionCurve::INSTANT) {
  return;
}
```

**Impact:** Prevents crashes from array out-of-bounds access when profile is empty or invalid.

---

### 6. **Improved Steam Control Comment**
**File:** `src/functional/just_do_coffee.cpp` (lines 103-108)  
**Severity:** Low  
**Issue:** Comment was misleading about when steam valve is turned off.

**Problem:**
Comment said "Steam relays kept off in brew mode" but condition also turns it off when brew switch is not active.

**Fix:**
Clarified comment to accurately describe the logic:
```cpp
// Steam relays kept off in brew mode or when brew switch is not active
// This ensures steam valve is only active during explicit steam mode, not during brewing
if (brewActive || !currentState.brewSwitchState) {
  setSteamValveRelayOff();
}
```

**Impact:** Better code documentation for maintainability.

---

## Testing Recommendations

1. **Ramp Phase Logic:** Test profile creation with first phase at index 0 to verify ramp insertion works correctly.
2. **Temperature Calculations:** Test with edge case temperature values and invalid setpoint configurations.
3. **Flow Calculations:** Test pump control at extreme pressure values (very low and very high) to verify fallback behavior.
4. **Empty Profile:** Test brew startup with empty or invalid profile to verify no crashes occur.
5. **Hot Water Mode:** Verify heater control works correctly without redundant calls.

---

## Code Quality Notes

All fixes follow defensive programming practices:
- ✅ Added bounds checking before array access
- ✅ Added division-by-zero protection
- ✅ Improved code clarity and documentation
- ✅ Maintained existing functionality while fixing bugs
- ✅ All changes compile without warnings or errors

These bugs were subtle logic errors that could cause crashes or incorrect behavior under specific conditions but were unlikely to be caught during normal operation.

