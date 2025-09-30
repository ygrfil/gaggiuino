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
  static bool filterInit = false;
  static float filteredTempC = 0.0f;
  if (!filterInit) {
    filteredTempC = tempC;
    filterInit = true;
  } else {
    // alpha = 0.18 ~ gentle smoothing
    filteredTempC = 0.82f * filteredTempC + 0.18f * tempC;
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

    // Far below: heat aggressively
    if (diff > 8.0f) {
      timePropHeat(1000, 100);
    } else if (diff > 4.0f) {
      timePropHeat(2000, 65);
    } else if (diff > 2.0f) {
      timePropHeat(3000, 40);
    } else if (diff > 1.0f) {
      // Near target: reduce power, preemptively cut if rising fast
      if (slopeCps > 0.25f) setBoilerOff(); else timePropHeat(4000, 20);
    } else if (diff > 0.5f) {
      if (slopeCps > 0.20f) setBoilerOff(); else timePropHeat(5000, 12);
    } else if (diff > 0.2f) {
      if (slopeCps > 0.15f) setBoilerOff(); else timePropHeat(6000, 6);
    } else if (diff > 0.0f) {
      if (slopeCps > 0.10f) setBoilerOff(); else timePropHeat(7000, 3);
    } else {
      // At/above target: off
      setBoilerOff();
    }
  }

  // Steam relays kept off in brew mode
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
  currentState.steamSwitchState ? lcdTargetState((int)HEATING::MODE_steam) : lcdTargetState((int)HEATING::MODE_brew); // setting the steam/hot water target temp
  // steam temp control, needs to be aggressive to keep steam pressure acceptable
  float steamTempSetPoint = runningCfg.steamSetPoint + runningCfg.offsetTemp;
  float sensorTemperature = currentState.temperature + runningCfg.offsetTemp;

#if defined(USE_HARDWARE_TIMER_PWM)
  // Use hardware timer for precise steam temperature control
  const int16_t TEMP_SCALE = 10;
  int16_t scaledSteamTempSetPoint = steamTempSetPoint * TEMP_SCALE;
  int16_t scaledSensorTemperature = sensorTemperature * TEMP_SCALE;
  
  if (currentState.smoothedPressure > steamThreshold_ || scaledSensorTemperature > scaledSteamTempSetPoint) {
    // Pressure or temperature too high - turn everything off
    heaterHardwareOff();
    setSteamBoilerRelayOff();
    setSteamValveRelayOff();
    setPumpOff();
  } else {
    // Apply proportional control for steam temperature
    const int16_t STEAM_TEMP_HYSTERESIS = 50; // 5.0 degrees
    
    // Calculate duty cycle based on temperature difference
    int16_t tempDiff = scaledSteamTempSetPoint - scaledSensorTemperature;
    uint8_t dutyCycle = 0;
    
    if (tempDiff <= 0) {
      // At or above target temperature
      dutyCycle = 0;
    } else if (tempDiff >= STEAM_TEMP_HYSTERESIS) {
      // Far below target temperature
      dutyCycle = 100;
    } else {
      // Proportional control when within hysteresis range
      dutyCycle = (tempDiff * 100) / STEAM_TEMP_HYSTERESIS;
    }
    
    // Apply PWM duty cycle
    setHeaterDutyCycle(dutyCycle);
    
    // Rest of steam control logic
    setSteamValveRelayOn();
    setSteamBoilerRelayOn();
    #ifndef DREAM_STEAM_DISABLED // disabled for bigger boilers which have no need of adding water during steaming
      if (currentState.smoothedPressure < activeSteamPressure_) {
        setPumpToRawValue(3);
      } else {
        setPumpOff();
      }
    #endif
  }
#else
  // Original steam control implementation
  if (currentState.smoothedPressure > steamThreshold_ || sensorTemperature > steamTempSetPoint) {
    setBoilerOff();
    setSteamBoilerRelayOff();
    setSteamValveRelayOff();
    setPumpOff();
  } else {
    if (sensorTemperature < steamTempSetPoint) {
      setBoilerOn();
    } else {
      setBoilerOff();
    }
    setSteamValveRelayOn();
    setSteamBoilerRelayOn();
    #ifndef DREAM_STEAM_DISABLED // disabled for bigger boilers which have no  need of adding water during steaming
      if (currentState.smoothedPressure < activeSteamPressure_) {
        setPumpToRawValue(3);
      } else {
        setPumpOff();
      }
    #endif
  }
#endif

  /*In case steam is forgotten ON for more than 15 min*/
  if (currentState.smoothedPressure > passiveSteamPressure_) {
    currentState.isSteamForgottenON = millis() - steamTime >= STEAM_TIMEOUT;
  } else steamTime = millis();
}

/*Water mode and all that*/
void hotWaterMode(const SensorState &currentState) {
  closeValve();
  setPumpToRawValue(80);
  setBoilerOn();
  if (currentState.temperature < MAX_WATER_TEMP) setBoilerOn();
  else setBoilerOff();
}

// Removed PID helper functions to simplify and match original behavior
