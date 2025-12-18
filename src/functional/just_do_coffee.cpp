/* 09:32 15/03/2023 - change triggering comment */
#include "just_do_coffee.h"
#include "../lcd/lcd.h"
#include "../peripherals/heater_control.h"
#include "../../lib/Common/system_state.h"

extern SystemState systemState;

extern unsigned long steamTime;

// Original control does not rely on persistent PID state


void justDoCoffee(const eepromValues_t &runningCfg, const SensorState &currentState, const bool brewActive) {
  // Brew mode target on LCD
  lcdTargetState((int)HEATING::MODE_brew);

  // Control inputs
  const float setpointC = ACTIVE_PROFILE(runningCfg).setpoint;
  const float tempC = currentState.temperature;

  // Low-pass filter to reduce noise and chatter
  // Initialize with first valid reading (not zero) for faster startup convergence
  static bool filterInit = false;
  static float filteredTempC = 0.0f;
  if (!filterInit && tempC > 20.0f) { // Wait for valid reading (above room temp)
    filteredTempC = tempC;
    filterInit = true;
  } else if (filterInit) {
    // alpha = 0.25 ~ slightly more responsive for tighter control
    filteredTempC = 0.75f * filteredTempC + 0.25f * tempC;
  } else {
    // Not yet initialized, use raw temperature
    filteredTempC = tempC;
  }

  // Timing and slope estimation
  static uint32_t lastTs = 0;
  static float lastFiltTemp = 0.0f;
  uint32_t now = millis();
  float slopeCps = 0.0f;
  if (lastTs != 0) {
    float dt = (now - lastTs) / 1000.0f;
    if (dt > 0.0f) slopeCps = (filteredTempC - lastFiltTemp) / dt;
  }
  lastFiltTemp = filteredTempC;
  lastTs = now;

  // Simple time-proportional heater control near setpoint
  auto timePropHeat = [](uint32_t periodMs, uint8_t dutyPct) {
    static uint32_t windowStart = 0;
    uint32_t t = millis();
    if (t - windowStart >= periodMs) windowStart = t;
    if (dutyPct == 0) { setBoilerOff(); return; }
    if (dutyPct >= 100) { setBoilerOn(); return; }
    uint32_t onTime = (periodMs * dutyPct) / 100u;
    if ((t - windowStart) < onTime) setBoilerOn(); else setBoilerOff();
  };

  // Safety: standby always forces heater off
  if (systemState.shutdownActive) {
    setBoilerOff();
  } else {
    const float diff = setpointC - filteredTempC; // positive when below target

    // Hardware-optimized control for Gaggia Classic (small boiler, 1400W element)
    // Goal: ±0.5°C stability (limited by boiler thermal mass and SSR minimum on-time)
    if (diff > 4.0f) {
      // Far below: heat aggressively
      timePropHeat(1000, 100);
    } else if (diff > 2.0f) {
      // Getting closer: reduce power
      timePropHeat(2000, 55);
    } else if (diff > 1.0f) {
      // Approaching target: moderate power with long period
      timePropHeat(3500, 28);
    } else if (diff > 0.5f) {
      // Close to target: very gentle with longer period to avoid SSR chatter
      // Use 6-second window for stable minimum duty cycle
      if (slopeCps > 0.15f) setBoilerOff(); else timePropHeat(6000, 12);
    } else if (diff > 0.0f) {
      // Within ±0.5°C deadband below setpoint: minimal pulses
      // 8-second window allows SSR to work reliably at minimum duty
      if (slopeCps > 0.08f) {
        setBoilerOff();
      } else {
        timePropHeat(8000, 8);  // ~640ms on per 8s = minimum practical SSR duty
      }
    } else if (diff > -0.5f) {
      // Within ±0.5°C deadband above setpoint: coast/maintain
      // Only heat if falling rapidly
      if (slopeCps < -0.10f) {
        timePropHeat(10000, 6);  // Very minimal maintenance heat
      } else {
        setBoilerOff();
      }
    } else {
      // Above deadband: off
      setBoilerOff();
    }
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
      // DreamSteam: add water when pressure is low
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
