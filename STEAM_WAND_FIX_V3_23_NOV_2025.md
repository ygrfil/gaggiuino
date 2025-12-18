# Steam Wand Pressure Fix V3 - November 23, 2025

## Issue
User reported "same problem, it doesnt fix anything". "valve of steam want is opend manualy, rethink".
Symptom: High pressure and temperature on screen, but "almost no pressure" from wand.

## Root Cause Analysis
The user's feedback points to a classic **Trapped Pressure** scenario:
1. The Pressure Sensor is located *before* the check valve (standard Gaggiuino mod).
2. The check valve holds the boiler pressure back.
3. The sensor reads line pressure, which can be high (e.g., 9 bar from a previous shot) and "Trapped".
4. When the manual steam wand valve is opened, the **Boiler** pressure drops, but the **Sensor** (Line) pressure does *not* drop because the check valve isolates it.
5. The pump logic `if (pressure < 2.5)` sees the High Trapped Pressure and **never turns on**.
6. Result: Boiler runs dry, temperature stays high, but no steam is generated.

## Solution V3

### 1. Smart "Trapped Pressure" Bypass
We cannot rely solely on the pressure sensor because it might be "blinded" by trapped pressure.
We now use a **Hybrid Trigger**:
- **Primary (Pressure):** If `pressure < 3.5 bar` (raised from 2.5), Pump runs (standard refill).
- **Secondary (Temperature):** If `Temp > 110°C` (Steam range) **AND** `Temp < Setpoint - 1°C` (Dropping/Heating), we assume steam is being used (heat loss = steam loss) and **pulse the pump** (`Power 32`).

This ensures that even if the pressure sensor reads 9 bar (trapped), the pump will still inject water as long as the heater is working to maintain steam temperature.

### 2. Manual Valve Acknowledgement
- Acknowledged that the system does not control the steam wand valve.
- Retained `closeValve()` to ensure the group head solenoid is sealed, directing all steam to the manual wand.

## Impact
- **Fail-Safe Refill**: Boiler will be replenished based on thermal load even if pressure sensor is isolated.
- **Continuous Steam**: Prevents the "dry boiler" scenario.
- **Safety**: Pump only pulses if temperature is high (>110°C), preventing accidental overfilling when cold.

## Firmware
The fix is applied in `src/functional/just_do_coffee.cpp`.
Flash the new firmware to apply the fix.


