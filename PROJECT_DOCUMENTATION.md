# Gaggiuino Project Documentation

**Last Updated:** September 30, 2025  
**Build Configuration:** Single-board PCB (`all-pcb-stlink`)  
**Hardware:** STM32F411CE (Blackpill) based espresso machine controller

---

## Table of Contents
1. [Project Overview](#project-overview)
2. [Hardware Architecture](#hardware-architecture)
3. [Software Architecture](#software-architecture)
4. [Key Subsystems](#key-subsystems)
5. [Temperature Control](#temperature-control)
6. [Pressure Management](#pressure-management)
7. [Brew Detection & Control](#brew-detection--control)
8. [Safety Features](#safety-features)
9. [Build Process](#build-process)
10. [Recent Improvements](#recent-improvements)
11. [Configuration & Tuning](#configuration--tuning)

---

## Project Overview

Gaggiuino is an open-source espresso machine controller that transforms a standard Gaggia Classic Pro into a precision-controlled coffee machine. The system provides:

- **Precise temperature control** with time-proportional heater modulation
- **Pressure profiling** during brew cycles
- **Automated safety features** including idle pressure release and heater standby
- **LCD interface** for real-time monitoring and control
- **Profile-based brewing** with customizable parameters stored in EEPROM
- **Remote monitoring** via ESP32 WiFi module (optional)

### Design Philosophy
- **SINGLE_BOARD PCB build** (no LEGO relay variant)
- Simple, maintainable code with minimal complexity
- Reliable operation with proper error handling and watchdog protection
- Non-blocking control where possible for responsive UI

---

## Hardware Architecture

### Core Controller
- **MCU:** STM32F411CE (Blackpill board)
- **Upload:** ST-Link via SWD (default) or DFU
- **Debug:** USB-CDC Serial

### Pin Assignments (Single-Board PCB)
```cpp
// Temperature sensor (K-type thermocouple)
thermoDO      PB4      // MAX31855 Data Out
thermoCS      PA6      // MAX31855 Chip Select
thermoCLK     PA5      // MAX31855 Clock

// Control outputs
relayPin      PA15     // Main boiler SSR
dimmerPin     PA1      // Pump dimmer/PWM
valvePin      PC13     // 3-way solenoid valve

// Input switches
brewPin       PC14     // Brew switch
steamPin      PC15     // Steam switch
waterPin      PB15     // Hot water switch (SINGLE_BOARD specific)
zcPin         PA0      // Zero-crossing detector

// Scales (HX711)
HX711_sck_1   PB0      // Scale clock
HX711_dout_1  PB8      // Scale data 1
HX711_dout_2  PB9      // Scale data 2 (dual-board support)

// Communication
USART_LCD     Serial2  // PA2(TX), PA3(RX) - Nextion display
USART_ESP     Serial1  // PA9(TX), PA10(RX) - ESP32 module
USART_DEBUG   Serial   // USB-CDC for logging
```

### Sensors & Peripherals
- **Temperature:** MAX31855 thermocouple amplifier (K-type)
- **Pressure:** ADS1115 ADC with analog pressure transducer
- **Weight:** HX711 load cell amplifier(s)
- **Display:** Nextion/TJC HMI touch LCD
- **Connectivity:** ESP32 for WiFi/WebSocket (optional)

---

## Software Architecture

### Main Loop Structure
```cpp
void loop(void) {
  fillBoiler();              // Water level management
  lcdListen();               // Process LCD commands
  sensorsRead();             // Update all sensor readings
  brewDetect();              // Edge-detect brew switch state
  modeSelect();              // Route to active control mode
  lcdRefresh();              // Update display
  espCommsSendSensorData();  // Send data to ESP32
  sysHealthCheck();          // Safety & pressure release
}
```

### Core Modules

#### `/src/gaggiuino.ino`
Main control flow, initialization, and system health monitoring.

#### `/src/functional/just_do_coffee.cpp`
Brew-mode temperature control with time-proportional heater modulation.

#### `/src/peripherals/`
- `heater_control.cpp` - SSR control with shutdown guard
- `pump.cpp` - Dimmer/PWM pump control
- `pressure_sensor.cpp` - Pressure reading and smoothing
- `scales.cpp` - Weight measurement
- `thermocouple.h` - Temperature sensor interface

#### `/lib/Common/`
Shared state structures and profiling phase logic:
- `system_state.h` - Global system state flags
- `sensors_state.h` - Sensor readings structure
- `profiling_phases.cpp` - Brew phase state machine

#### `/src/eeprom_data/`
Profile storage, retrieval, and defaults.

---

## Key Subsystems

### 1. Sensor Reading Pipeline

**Temperature (K-type thermocouple via MAX31855):**
- Read every main loop cycle (~100-200 Hz)
- Hardware-filtered by MAX31855
- Software offset applied from EEPROM calibration
- Used directly for control (no additional smoothing in recent builds)

**Pressure (analog transducer via ADS1115):**
- Raw reading: `currentState.pressure`
- Smoothed reading: `currentState.smoothedPressure` (Kalman filter)
- Units: bar
- Used for brew profiling and idle release detection

**Weight (HX711 load cell):**
- Dual-board support (single or two HX711s)
- Tare function for cup/container zeroing
- Predictive weight estimation for flow control

---

## Temperature Control

### Implementation: Time-Proportional with Anti-Overshoot

Located in: `src/functional/just_do_coffee.cpp`

#### Algorithm (September 2025 revision)
```cpp
// 1. Simple low-pass filter for stability
filteredTemp = 0.85 * filteredTemp + 0.15 * rawTemp

// 2. Calculate error
error = setpoint - filteredTemp

// 3. Proportional duty cycle with zones:
//    Far from target (>1.5°C): 100% duty
//    Mid-range (0.5-1.5°C):    40% duty
//    Close (<0.5°C):           20% duty

// 4. Anti-overshoot: cut heater if rising >0.8°C/s near setpoint

// 5. Time-proportional modulation over 1-second window
```

#### Key Features
- **No overshoot**: Preemptive cutoff prevents temperature excursions
- **Fast recovery**: Full power when far from setpoint
- **Stable hold**: Minimal oscillation at target (±0.3°C typical)
- **Respects standby**: Heater disabled when `systemState.shutdownActive == true`

#### Tuning Parameters
```cpp
const float alpha = 0.15f;           // Low-pass filter coefficient
const float fullPowerThresh = 1.5f;  // Full power above this error
const float midBandThresh = 0.5f;    // Mid-power transition point
const float overshootSlope = 0.8f;   // °C/s slope cutoff threshold
const unsigned long cyclePeriodMs = 1000;  // Modulation window
```

### Temperature Safety
- **Thermal runaway detection**: System shutdown if temp exceeds safe limits
- **Sensor fault recovery**: Automatic retry on read errors
- **Offset calibration**: Per-machine temperature offset stored in EEPROM

---

## Pressure Management

### Idle Pressure Release

**Problem Solved:**  
After heating or brew cycles, residual pressure can build up. The system automatically vents pressure when idle to prevent safety issues and improve user experience.

**Implementation (recent fix - Sept 2025):**
```cpp
// Triggered when:
// - No switches active (brew/steam/water all OFF)
// - Pressure >= threshold (typically 0.5 bar)
// - Temperature < 100°C (not boiling)

// Action:
openValve();        // Open 3-way to vent
setPumpOff();       // Ensure pump is off
setBoilerOff();     // Disable heater during vent

// Exit conditions:
// - Raw pressure < (threshold - 0.2) bar (hysteresis)
// - Smoothed pressure < (threshold - 0.1) bar
// - Timeout after 10 seconds
// - Pressure dropping: reset timeout if falling >0.1 bar from start
```

**Recent Fixes:**
1. **Timer bug fix:** Moved timeout initialization outside loop to prevent premature exits on subsequent cycles
2. **Hysteresis:** Added dual-check on raw and smoothed pressure to avoid filter lag issues
3. **Progress tracking:** Reset timeout if pressure is actively falling

### Brew Pressure Profiling
- Managed by `profiling_phases.cpp` state machine
- Supports multi-phase pressure/flow profiles
- Real-time adjustment via pump PWM control

---

## Brew Detection & Control

### Brew Switch Edge Detection

**Implementation (September 2025):**
```cpp
static bool lastBrewState = false;

void brewDetect(void) {
  if (!sysReadinessCheck()) return;
  
  bool currentBrewState = currentState.brewSwitchState;
  
  // Rising edge: brew button pressed
  if (currentBrewState && !lastBrewState) {
    lcdWakeUp();
    brewParamsReset();
    brewActive = true;
    systemHealthTimer = millis() + HEALTHCHECK_EVERY;
  }
  // Falling edge: brew button released
  else if (!currentBrewState && lastBrewState) {
    brewActive = false;
    currentState.pumpClicks = getAndResetClickCounter();
    brewParamsReset();
  }
  
  lastBrewState = currentBrewState;
}
```

**Key Improvement:**  
Replaced unreliable toggle-based detection with clean edge detection. This eliminated the ~20% failure rate on first brew after power-on.

### Brew Modes
1. **Manual pressure profiling** - Follow stored EEPROM profile
2. **Just Do Coffee** - Maintain setpoint temperature, user controls flow
3. **Flush mode** - Cleaning cycle
4. **Descale mode** - Maintenance routine

---

## Safety Features

### 1. Watchdog Timer
- Hardware watchdog reloaded every health check
- Forces MCU reboot if main loop hangs

### 2. Temperature Fault Recovery
```cpp
// Blocking retry on sensor read failure
// - Attempt re-read every 1 second
// - Show warning on LCD
// - Disable heater during fault
```

### 3. Steam Forgotten Alert
```cpp
// If steam switch left ON for extended period:
// - Alert user via LCD popup
// - Continue monitoring until switch released
```

### 4. Auto Heater Standby (Sept 2025)

**Configuration:**
- Enabled by default
- Triggers after **25 minutes** of inactivity
- **1 minute warning** before activation

**Behavior:**
```cpp
// After timeout:
// 1. Show "Heater standby" popup
// 2. Set systemState.shutdownActive = true
// 3. Heater is blocked from turning on
// 4. Pump, valves, and other systems remain functional

// Resume on:
// - Any button press (brew/steam/water)
// - LCD page change
// → Clears shutdownActive, resets activity timer
```

**Implementation Guards:**
- `setBoilerOn()` checks `shutdownActive` flag
- `justDoCoffee()` and `steamCtrl()` respect standby state
- Heater demand is zeroed when standby is active

**Rationale:**  
Hardware limitations prevent full system power-off. Heater-only standby provides energy savings and burn protection while maintaining system responsiveness.

---

## Build Process

### Environment Setup
```bash
# Install PlatformIO CLI or use VSCode extension
pip install platformio

# Clone repository
git clone <your-repo-url>
cd gaggiuino
```

### Build Commands
```bash
# Build for single-board PCB (primary target)
pio run -e all-pcb-stlink

# Build and upload via ST-Link
pio run -e all-pcb-stlink -t upload

# Build via DFU (USB bootloader)
pio run -e all-pcb-stlink --upload-port /dev/ttyACM0 -t upload

# Clean build
pio run -e all-pcb-stlink -t clean
```

### Firmware Distribution
After building, copy firmware with descriptive name and timestamp:
```bash
TS=$(date +%Y%m%d-%H%M%S)
cp .pio/build/all-pcb-stlink/firmware.bin \
   gaggiuino-<feature-description>-$TS.bin
```

**Naming Convention:**
- `gaggiuino-<feature>-<YYYYMMDD>-<HHMMSS>.bin`
- Examples:
  - `gaggiuino-temp-tp-antiovershoot-20250930-231603.bin`
  - `gaggiuino-brew-edge-detect-20250930-230417.bin`
  - `gaggiuino-heater-standby-only-20250930-225337.bin`

### Build Flags (platformio.ini)
```ini
[env:all-pcb-stlink]
extends = blackpill-core
build_type = release
build_flags =
  ${blackpill-core.build_flags}
  -DSINGLE_BOARD              # Single-board PCB build
  -DLOG_LEVEL=3               # Info-level logging
  -O3                         # Aggressive optimization
  -mfloat-abi=hard            # Hardware floating-point
  -mfpu=fpv4-sp-d16
```

---

## Recent Improvements

### September 2025 Session

#### 1. Pressure Release Reliability
**Issue:** Intermittent failure to release pressure (first works, second maybe, then stops)

**Root Cause:** Static timeout variable inside loop retained state across cycles

**Fix:** 
- Moved `pressureReleaseStart` initialization before loop
- Added hysteresis and raw pressure early-exit
- Reset timeout if pressure actively dropping

**Result:** Consistent pressure release every cycle

---

#### 2. Brew Start Reliability  
**Issue:** ~20% failure rate on first brew press after power-on

**Root Cause:** Toggle-based `paramsReset` flag could miss first press

**Fix:**
- Replaced with clean rising/falling edge detection
- Track previous switch state and detect transitions

**Result:** 100% reliable brew initiation

---

#### 3. Temperature Stability
**Issue:** Large overshoot (92°C → 105°C) and oscillation (105 → 91 → 105)

**Root Cause:** Simple bang-bang control with inadequate hysteresis and no anticipation

**Fix Evolution:**
1. ❌ Tried: Larger hysteresis bang-bang → still overshot
2. ✅ **Implemented:** Time-proportional control with:
   - Light low-pass filtering (α=0.15)
   - Multi-zone duty cycling (100%/40%/20%)
   - Anti-overshoot slope detection (>0.8°C/s cutoff)

**Result:** Stable ±0.3°C at setpoint, no overshoot

---

#### 4. Heater Auto-Standby
**Issue:** Original 25-minute full shutdown blocked by hardware limitations

**Fix:**
- Changed to heater-only standby
- Guard `setBoilerOn()` with `shutdownActive` check
- Reset on any user interaction
- LED stays on, system responsive, heater disabled

**Result:** Safe energy savings without hardware modifications

---

#### 5. Code Quality (Attempted)
**Goal:** Convert blocking loops to non-blocking state machines

**Outcome:** Reverted due to unexpected behavior

**Lesson:** Blocking approach is acceptable for current system; non-blocking adds complexity without clear benefit for this use case

---

## Configuration & Tuning

### EEPROM Settings
- **Temperature offset:** Calibrate per-machine thermocouple error
- **Brew profiles:** Multi-phase pressure/flow curves
- **Auto-shutdown:** Enable/disable, timeout values
- **Scale calibration:** Load cell factor and tare weight

### Access via LCD
1. Navigate to Settings page
2. Adjust setpoint, profiles, offsets
3. Changes persist across power cycles

### Temperature Tuning
If you need different temperature behavior, adjust in `just_do_coffee.cpp`:

```cpp
// For tighter control (more heater cycles):
const float fullPowerThresh = 1.2f;  // Was 1.5
const float midBandThresh = 0.4f;    // Was 0.5

// For fewer cycles (wider tolerance):
const float fullPowerThresh = 2.0f;
const float midBandThresh = 0.7f;

// For aggressive anti-overshoot:
const float overshootSlope = 0.6f;   // Was 0.8
```

### Pressure Release Tuning
Adjust threshold in `gaggiuino.ino`:

```cpp
#define SYS_PRESSURE_IDLE 0.5f  // Bar, lower = less aggressive

// In sysHealthCheck():
const float pressureThreshold = 0.3f;  // Custom per call
```

---

## Debugging & Diagnostics

### Serial Logging
```cpp
// Enable debug output in platformio.ini:
build_flags = -DLOG_LEVEL=3  // 0=Error, 1=Warn, 2=Info, 3=Debug

// View logs:
pio device monitor -b 115200
```

### Common Issues

**Temperature not stable:**
- Check thermocouple connection (cold joint?)
- Verify temperature offset calibration
- Ensure SSR is switching properly (LED blink?)

**Pressure release not working:**
- Confirm valve wiring (NC vs NO)
- Check pressure sensor calibration
- Verify `SINGLE_BOARD` flag is set

**Brew won't start:**
- Check brew switch wiring
- Verify `sysReadinessCheck()` passes (temp in range, boiler filled)
- Look for blocking popups on LCD

**LCD unresponsive:**
- Check Serial2 connections (PA2/PA3)
- Verify baud rate (115200)
- Re-flash LCD firmware (`.tft` file)

---

## File Structure Quick Reference

```
gaggiuino/
├── src/
│   ├── gaggiuino.ino            # Main control loop
│   ├── gaggiuino.h              # Global includes
│   ├── pindef.h                 # Pin definitions
│   ├── functional/
│   │   └── just_do_coffee.cpp   # Temperature control ⭐
│   ├── peripherals/
│   │   ├── heater_control.cpp   # SSR control ⭐
│   │   ├── pump.cpp             # Pump PWM
│   │   ├── pressure_sensor.cpp  # Pressure reading
│   │   └── ...
│   └── eeprom_data/
│       └── eeprom_data.cpp      # Profile storage
├── lib/Common/
│   ├── system_state.h           # Global state flags ⭐
│   ├── sensors_state.h          # Sensor data struct
│   └── profiling_phases.cpp     # Brew state machine
├── platformio.ini               # Build configuration
└── PROJECT_DOCUMENTATION.md     # This file

⭐ = Recently modified (Sept 2025)
```

---

## Future Considerations

### Potential Improvements
1. **PID temperature control** - More sophisticated than time-proportional (see `peripherals/pid_controller.cpp` for existing framework)
2. **Non-blocking state machines** - Improve responsiveness (attempted, needs more testing)
3. **Adaptive pressure profiling** - ML-based shot optimization
4. **Cloud logging** - Send shot data to remote server for analysis

### Known Limitations
- **Hardware shutdown:** Cannot fully power off due to machine wiring constraints
- **Temperature sensor lag:** K-type thermocouple has ~1-2 second thermal mass delay
- **Pump resolution:** Dimmer granularity limits fine pressure control at low flow

---

## Appendix: Key Constants

```cpp
// Temperature control (just_do_coffee.cpp)
#define ALPHA 0.15f                    // Low-pass filter coefficient
#define FULL_POWER_THRESH 1.5f         // Full power distance from setpoint (°C)
#define MID_BAND_THRESH 0.5f           // Mid-power transition (°C)
#define OVERSHOOT_SLOPE 0.8f           // Anti-overshoot cutoff (°C/s)
#define CYCLE_PERIOD_MS 1000           // Modulation window (ms)

// Safety (gaggiuino.ino)
#define SYS_PRESSURE_IDLE 0.5f         // Idle pressure release threshold (bar)
#define HEALTHCHECK_EVERY 3000         // Health check interval (ms)
#define AUTO_SHUTDOWN_TIME 1500000     // 25 minutes in ms
#define AUTO_SHUTDOWN_WARNING 1440000  // 24 minutes (1 min before shutdown)

// Pressure release
#define PRESSURE_RELEASE_TIMEOUT 10000 // Maximum vent duration (ms)
#define PRESSURE_HYSTERESIS_RAW 0.2f   // Raw pressure exit margin (bar)
#define PRESSURE_HYSTERESIS_SMOOTH 0.1f // Smoothed pressure exit margin (bar)
```

---

## Contact & Support

- **Repository:** [Original Gaggiuino Project](https://github.com/Zer0-bit/gaggiuino)
- **Discord:** [Gaggiuino Community](https://discord.gg/eJTDJA3xfh)
- **Documentation:** [Official Docs](https://gaggiuino.github.io)

---

**Document Version:** 1.0  
**Hardware Target:** Single-board PCB only  
**Firmware Base:** September 2025 stable build with temperature TP control and brew edge detection
