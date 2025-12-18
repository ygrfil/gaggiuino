# Steam Wand Pressure Fix V2 - November 23, 2025

## Issue
User reported "same problem, it doesnt fix anything". Pressure and temperature go up, but opening the steam wand results in "almost no pressure".

## Root Causes Identified
1. **Pump Power Too Low**: The previously set pump power (`22`) was likely insufficient to push water into the boiler against the residual pressure (1-2 bar). This resulted in the pump stalling or not moving enough water to generate steam, leading to a "dry boiler" scenario where temperature is high but steam volume is low.
2. **Valve State Uncertainty**: While the system should close the 3-way solenoid valve, it wasn't explicitly enforced in the `steamCtrl` loop. If the valve remained partially open or leaked, steam pressure would be lost to the group head/drip tray instead of the wand.

## Solution V2

### 1. Increased Steam Pump Power
- Increased pump power from `22` to **`45`**. 
- This ensures the pump has sufficient torque to overcome boiler pressure and inject water reliably.
- This is safe because it only triggers when pressure is low (< 2.5 bar).

### 2. Enforced Valve Closure
- Added explicit `closeValve()` calls within the `steamCtrl` loop.
- This ensures the 3-way solenoid is strictly CLOSED, directing all pressure to the steam wand and preventing leaks to the group head.

### 3. Preserved Decoupled Logic
- Maintained the separation of Pump and Heater logic established in V1.
- Pump runs based on Pressure (< 2.5 bar).
- Heater runs based on Temperature (up to Setpoint + 5°C).
- Safety cutoff remains at 11 bar.

## Impact
- **Stronger Water Injection**: The pump will now reliably push water into the boiler when pressure drops, generating continuous steam.
- **Leak Prevention**: Explicit valve closure ensures pressure is built up in the boiler/wand circuit only.
- **Consistent Steam**: Prevents the "dry steam" issue by ensuring water is available to flash-boil.

## Firmware
The fix is applied in `src/functional/just_do_coffee.cpp`.
Flash the new firmware to apply the fix.


