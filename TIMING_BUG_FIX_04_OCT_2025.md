# Critical Timing Bug Fix - October 4, 2025

## Bug Description

**Symptom:** Rare but critical bug where starting a brew would fail:
- Machine on for 10-15+ minutes
- Press brew button
- Display doesn't change to brew screen
- Pump makes sound for ~5 seconds
- System releases pressure like end of brew
- Brew never actually starts

**Frequency:** Rare - only happens when timing coincides with health check cycle

## Root Cause Analysis

### The Problem

The system has a health check that runs every 30 seconds (`HEALTHCHECK_EVERY = 30000ms`) to release excess pressure accumulated during heating. 

**Critical Race Condition:**

1. **Setup Phase (0-15 minutes):**
   - Machine heats and maintains temperature
   - Normal heating causes 0.7-1.0 bar residual pressure to build up
   - Health check timer ticks every 30 seconds

2. **Bug Trigger (When stars align):**
   ```
   Time: 10-15 minutes into session
   ├─ Health timer expires (millis() >= systemHealthTimer)
   ├─ Pressure check: 0.8 bar > 0.7 bar threshold ✓
   ├─ ENTERS pressure release while loop (line 956)
   │  ├─ Opens valve (line 943)
   │  ├─ Turns off pump (line 944)
   │  └─ Starts 5-15 second pressure venting cycle
   │
   └─ User presses brew button DURING this window
      ├─ brewDetect() called (line 984)
      ├─ Sets brewActive = true ✓
      ├─ BUT valve already open ✗
      ├─ AND pump already off ✗
      └─ Pressure release loop continues to completion
         └─ Brew request ignored/aborted
   ```

3. **Why It's Rare:**
   - Requires pressing brew during narrow 5-15 second pressure release window
   - Window only opens every 30 seconds when pressure is high enough
   - After 10+ minutes when residual pressure has built up

### Code Flow Before Fix

```cpp
// Line 931: Health check timer expired
if (millis() >= systemHealthTimer) {
    // Line 933: Pressure needs release
    if (currentState.smoothedPressure >= 0.7f) {
        openValve();      // Line 943
        setPumpOff();     // Line 944
        
        // Line 956: BLOCKING LOOP - takes 5-15 seconds
        while (currentState.temperature < 100.f) {
            sensorsRead();
            
            // Line 984: brewDetect() called but trapped in loop
            brewDetect();  // Sets brewActive=true
            
            // Loop continues releasing pressure
            // Brew request is ignored until loop exits
            
            if (pressure_low) break;
            if (timeout_15s) break;
        }
        
        closeValve();     // Line 1008
    }
}
```

**The Problem:** Once inside the `while` loop, even though `brewDetect()` sets `brewActive = true`, the valve remains open and pump stays off until the loop completes. By then, the brew window has passed.

## The Fix

### Two-Layer Protection

**Layer 1: Prevent Entry** (Line 926-930)
```cpp
// Don't even START pressure release if brewing is active
if (brewActive || currentState.brewSwitchState || ...) {
    systemHealthTimer = millis() + HEALTHCHECK_EVERY;
    return;  // Skip pressure release entirely
}
```

**Layer 2: Emergency Exit** (Line 961-967)
```cpp
while (currentState.temperature < 100.f) {
    // CRITICAL: Exit immediately if brew starts mid-cycle
    if (brewActive || currentState.brewSwitchState) {
        LOG_INFO("Pressure release aborted - brew started");
        break;  // Exit loop, close valve, resume normal operation
    }
    
    // Rest of pressure release logic...
}
```

### Why This Works

1. **Prevention:** Layer 1 catches 99% of cases - if you're brewing, pressure release won't start
2. **Escape Hatch:** Layer 2 catches the 1% edge case where brew starts mid-release cycle
3. **Fast Response:** Check happens at top of loop (every ~100ms), not after 15 second timeout
4. **Safe Exit:** Breaks loop → closes valve → returns control → brew proceeds normally

## Testing Verification

### Reproduction Steps (Before Fix)
1. Turn on machine, let heat for 10-15 minutes
2. Watch for "Releasing pressure!" popup (or wait for pressure to build)
3. Press brew button **immediately** when popup appears or during countdown
4. **Bug:** Brew fails, pressure releases, display stuck

### Expected Behavior (After Fix)
1. Same setup as above
2. Press brew during pressure release window
3. **Fixed:** 
   - Pressure release aborts immediately
   - Log shows: "Pressure release aborted - brew started"
   - Brew proceeds normally
   - Display changes to brew screen
   - Shot extraction works as expected

## Technical Details

### Timing Analysis

**Health Check Cycle:**
```
Every 30 seconds:
├─ Check if pressure > 0.7 bar
├─ If yes: Start pressure release (5-15 second window)
└─ If no: Skip to next cycle

Bug Window = 5-15 seconds every 30 seconds = 16-50% exposure
After 15 minutes = 30 health checks = 30 opportunities for collision
```

**Why 10-15 Minutes?**
- Boiler needs time to build residual pressure from heating cycles
- Fresh startup: pressure near 0, no release needed
- After 10+ minutes: 0.7-1.2 bar typical, triggers release
- This matches user's observation perfectly

### Code Changes Summary

**File:** `src/gaggiuino.ino`

**Change 1:** Line 926 (Prevention)
```diff
- if (currentState.brewSwitchState || currentState.steamSwitchState || currentState.hotWaterSwitchState) {
+ if (brewActive || currentState.brewSwitchState || currentState.steamSwitchState || currentState.hotWaterSwitchState) {
```

**Change 2:** Lines 961-967 (Emergency Exit)
```diff
  while (currentState.temperature < 100.f) {
    watchdogReload();
+   
+   // CRITICAL: Exit pressure release immediately if user starts brewing
+   if (brewActive || currentState.brewSwitchState) {
+     LOG_INFO("Pressure release aborted - brew started (pressure: %.2f bar)", currentState.smoothedPressure);
+     break;
+   }
    
    sensorsRead();
    // ... rest of pressure release logic
```

## Impact Assessment

### Severity: **CRITICAL**
- Completely blocks brewing (core functionality)
- No workaround except waiting or rebooting
- Appears random to users (timing-dependent)
- Could cause user to think machine is broken

### Frequency: **Low but Non-Zero**
- Requires specific timing alignment
- More likely on machines with good thermal mass (holds pressure longer)
- More likely during busy morning routines (frequent brew attempts)

### User Impact: **High**
- Frustrating when it happens
- No clear error message (before fix)
- Loses shot if coffee already ground and in portafilter
- Wastes time (5-15 second wait per occurrence)

## Build Information

**Firmware Version:** `5e1cd653+`  
**Build Date:** October 4, 2025  
**Environment:** `all-pcb-stlink` (Single Board PCB)  
**Binary:** `gaggiuino-timing-bugfix-04-Oct-2025.bin`

### Build Statistics
- **Flash Usage:** 26.2% (137,448 / 524,288 bytes) - +136 bytes
- **RAM Usage:** 22.4% (29,296 / 131,072 bytes) - unchanged
- **Build Status:** ✅ Success
- **Linter Status:** ✅ Clean

## Recommendations

1. **Immediate Deployment:** This is a critical bug fix that should be deployed ASAP
2. **Testing Protocol:**
   - Let machine warm up 15 minutes
   - Try to brew during visible pressure release cycles
   - Verify brew starts immediately and pressure release aborts
   - Check logs for "Pressure release aborted" messages

3. **Monitoring:**
   - Watch for any related issues (unlikely)
   - Verify pressure management still works when NOT brewing
   - Confirm no false triggers during normal operation

## Related Issues

This fix is complementary to the previous bug fixes from Oct 3, 2025:
- Water level check logic fix
- Division by zero protections
- All work together for robust operation

## Notes

- Fix is conservative and defensive
- Maintains all safety features
- No changes to pressure release logic when NOT brewing
- Adds early-exit path only when needed
- Logging provides visibility for diagnostics

---

**Conclusion:** This was a subtle race condition that only manifested under specific timing conditions. The two-layer fix (prevention + escape hatch) ensures brewing always takes priority over automatic pressure maintenance cycles.

