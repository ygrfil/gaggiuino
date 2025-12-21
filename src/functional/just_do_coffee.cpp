/* 09:32 15/03/2023 - change triggering comment */
#include "just_do_coffee.h"
#include "../lcd/lcd.h"
#include "../peripherals/heater_control.h"
#include "../../lib/Common/system_state.h"

extern SystemState systemState;

extern unsigned long steamTime;

// SIMPLIFIED: Single fixed period for all temperature control
// This eliminates timing state corruption that caused the intermittent brew bug
static const uint32_t HEATER_PERIOD_MS = 2000;  // 2 second period - good balance for all conditions
static uint32_t g_heaterWindowStart = 0;

void justDoCoffee(const eepromValues_t &runningCfg, const SensorState &currentState, const bool brewActive) {
  // Brew mode target on LCD
  lcdTargetState((int)HEATING::MODE_brew);

  // Control inputs
  const float setpointC = ACTIVE_PROFILE(runningCfg).setpoint;
  const float tempC = currentState.temperature;

  // Low-pass filter to reduce noise and chatter
  static bool filterInit = false;
  static float filteredTempC = 0.0f;
  static uint32_t lastFilterUpdate = 0;
  
  uint32_t now = millis();
  
  // Reset filter if stale (>5 seconds gap indicates mode switch)
  if (filterInit && (now - lastFilterUpdate > 5000)) {
    filteredTempC = tempC;
  }
  lastFilterUpdate = now;
  
  if (!filterInit && tempC > 20.0f) {
    filteredTempC = tempC;
    filterInit = true;
  } else if (filterInit) {
    filteredTempC = 0.75f * filteredTempC + 0.25f * tempC;
  } else {
    filteredTempC = tempC;
  }

  // Slope estimation for predictive control
  static uint32_t lastTs = 0;
  static float lastFiltTemp = 0.0f;
  float slopeCps = 0.0f;
  
  if (lastTs != 0 && (now - lastTs) < 2000) {
    float dt = (now - lastTs) / 1000.0f;
    if (dt > 0.0f) slopeCps = (filteredTempC - lastFiltTemp) / dt;
  }
  lastFiltTemp = filteredTempC;
  lastTs = now;

  // SIMPLIFIED: Single time-proportional heater control with fixed period
  // No more period switching = no timing corruption bugs
  auto timePropHeat = [&now](uint8_t dutyPct) {
    // Handle window wrap-around
    if (now - g_heaterWindowStart >= HEATER_PERIOD_MS) {
      g_heaterWindowStart = now;
    }
    
    if (dutyPct == 0) { setBoilerOff(); return; }
    if (dutyPct >= 100) { setBoilerOn(); return; }
    
    uint32_t onTime = (HEATER_PERIOD_MS * dutyPct) / 100u;
    if ((now - g_heaterWindowStart) < onTime) setBoilerOn(); else setBoilerOff();
  };

  // Safety: standby always forces heater off
  if (systemState.shutdownActive) {
    setBoilerOff();
  } else {
    const float diff = setpointC - filteredTempC; // positive when below target

    // SIMPLIFIED temperature control for Gaggia Classic (small boiler, 1400W element)
    // Single 2-second period with variable duty cycle - simpler and more reliable
    // Slope-based predictive adjustment prevents overshoot
    
    uint8_t duty = 0;
    
    if (diff > 4.0f) {
      // Far below target: full power
      duty = 100;
    } else if (diff > 2.0f) {
      // Getting closer: high power, reduce if rising fast
      duty = (slopeCps > 0.3f) ? 40 : 60;
    } else if (diff > 1.0f) {
      // Approaching target: moderate power with slope adjustment
      duty = (slopeCps > 0.2f) ? 20 : 35;
    } else if (diff > 0.3f) {
      // Close to target: gentle heating unless already rising
      duty = (slopeCps > 0.1f) ? 0 : 15;
    } else if (diff > 0.0f) {
      // Very close below setpoint: minimal heat unless falling
      duty = (slopeCps > 0.05f) ? 0 : 8;
    } else if (diff > -0.5f) {
      // Slightly above setpoint: only heat if falling fast
      duty = (slopeCps < -0.1f) ? 5 : 0;
    } else {
      // Well above setpoint: off
      duty = 0;
    }
    
    timePropHeat(duty);
  }

  // Steam relays kept off in brew mode or when brew switch is not active
  // This ensures steam valve is only active during explicit steam mode, not during brewing
  if (brewActive || !currentState.brewSwitchState) {
    setSteamValveRelayOff();
  }
  setSteamBoilerRelayOff();
}

void pulseHeaters(const uint32_t pulseLength, const int factor_1, const int factor_2, const bool brewActive) {
#if defined(USE_HARDWARE_TIMER_PWM)
  // Use hardware timer based PWM for precise heater control
  configurePWMHeaterControl(pulseLength, factor_1, factor_2, brewActive);
#else
  // Original software timer based implementation
  static uint32_t heaterWave;
  static bool heaterState;
  if (!heaterState && ((millis() - heaterWave) > (pulseLength * factor_1))) {
    brewActive ? setBoilerOff() : setBoilerOn();
    heaterState=!heaterState;
    heaterWave=millis();
  } else if (heaterState && ((millis() - heaterWave) > (pulseLength / factor_2))) {
    brewActive ? setBoilerOn() : setBoilerOff();
    heaterState=!heaterState;
    heaterWave=millis();
  }
#endif
}

//#############################################################################################
//################################____STEAM_POWER_CONTROL____##################################
//#############################################################################################
void steamCtrl(const eepromValues_t &runningCfg, SensorState &currentState) {
  lcdTargetState((int)(currentState.steamSwitchState ? HEATING::MODE_steam : HEATING::MODE_brew));

  // CRITICAL: Close the 3-way valve to direct steam to the steam wand!
  // On SINGLE_BOARD the valvePin controls the 3-way solenoid:
  // - closeValve() = steam/water goes to steam wand
  // - openValve() = steam/water goes to group head (wrong for steaming!)
  closeValve();

  // steam temp control, needs to be aggressive to keep steam pressure acceptable
  float steamTempSetPoint = runningCfg.steamSetPoint + runningCfg.offsetTemp;
  float sensorTemperature = currentState.temperature + runningCfg.offsetTemp;

  // Steam logic - shut down if pressure or temp exceeds limits
  if (currentState.smoothedPressure > steamThreshold_ || sensorTemperature > steamTempSetPoint) {
    setBoilerOff();
    setSteamBoilerRelayOff();
    setSteamValveRelayOff();
    setPumpOff();
  } else {
    // Control boiler based on temperature
    (sensorTemperature < steamTempSetPoint) ? setBoilerOn() : setBoilerOff();
    setSteamValveRelayOn();
    setSteamBoilerRelayOn();
    #ifndef DREAM_STEAM_DISABLED
      // DreamSteam: add water when pressure is low to enable continuous steaming
      // Pump runs at very low power (3) to slowly replenish boiler water
      (currentState.smoothedPressure < activeSteamPressure_) ? setPumpToRawValue(3) : setPumpOff();
    #endif
  }

  /*In case steam is forgotten ON for more than 15 min*/
  if (currentState.smoothedPressure > passiveSteamPressure_) {
    currentState.isSteamForgottenON = millis() - steamTime >= STEAM_TIMEOUT;
  } else steamTime = millis();
}

/*Water mode and all that*/
void hotWaterMode(const SensorState &currentState) {
  closeValve();
  setPumpToRawValue(80);
  // CRITICAL FIX: Remove redundant setBoilerOn() call - only set based on temperature
  if (currentState.temperature < MAX_WATER_TEMP) {
    setBoilerOn();
  } else {
    setBoilerOff();
  }
}

// Removed PID helper functions to simplify and match original behavior
