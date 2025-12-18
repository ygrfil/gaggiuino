# Steam Wand Pressure Fix - November 23, 2025

## Issue
User reported "pressure and temperature go up, but when i open steam wand the steam go with almost no pressure".

## Root Cause
The steam control logic (`steamCtrl` in `src/functional/just_do_coffee.cpp`) had a condition that disabled **ALL** components (Heater, Pump, Valves) if the temperature reached the setpoint (`sensorTemperature > steamTempSetPoint`).

This created a deadlock situation:
1. Boiler heats up to setpoint (e.g. 140°C).
2. Heater turns off (Correct).
3. **Pump turns off** (Incorrect).
4. User opens steam wand -> Pressure drops.
5. Even if pressure drops to 0 bar, the pump would **not** turn on to refill the boiler because the temperature was still above the setpoint (due to thermal mass).
6. Result: Boiler runs dry of steam/water, pressure drops to zero, and no new steam is generated until temperature drops significantly (which takes time).

Additionally, the pump power (`15`) and pressure threshold (`2.0 bar`) were likely too conservative for some setups, failing to inject water against boiler pressure.

## Solution

### 1. Decoupled Pump Logic from Heater Logic
Refactored `steamCtrl` to separate the control loops:
- **Heater**: Controls based on Temperature (keeps heating until Setpoint + 5°C hysteresis).
- **Pump**: Controls based on **Pressure** (injects water if pressure < 2.5 bar).
- **Safety**: Global cutoff only triggers if pressure exceeds safety limit (`steamThreshold_` = 11 bar).

Now, if the boiler is hot (140°C) but pressure is low (e.g. 1.5 bar) due to steam usage, the pump **will** activate to inject water. This water will flash-boil (generating steam) and cool the boiler slightly (triggering the heater), maintaining continuous steam pressure.

### 2. Enhanced Steam Boost Parameters
- **Pump Threshold**: Increased from `2.0 bar` to **`2.5 bar`**. The pump now kicks in earlier to maintain higher pressure.
- **Pump Power**: Increased from `15` to **`22`**. This ensures the pump has enough torque to push water into the pressurized boiler.

## Impact
- Continuous steam pressure even when boiler is fully heated.
- Faster recovery of steam pressure during use.
- Prevents "dry steam" or "no pressure" scenarios when boiler is hot but empty.

## Firmware
The fix is applied in `src/functional/just_do_coffee.cpp`.
Flash the new firmware to apply the fix.


