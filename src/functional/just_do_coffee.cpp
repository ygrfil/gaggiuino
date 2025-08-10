/* 09:32 15/03/2023 - change triggering comment */
#include "just_do_coffee.h"
#include "../lcd/lcd.h"
#include "../peripherals/heater_control.h"

extern unsigned long steamTime;

// Original control does not rely on persistent PID state


void justDoCoffee(const eepromValues_t &runningCfg, const SensorState &currentState, const bool brewActive) {
  // Set target mode to brew temp
  lcdTargetState((int)HEATING::MODE_brew);
  
  // Use consistent units: setpoint in °C from profile; sensor already offset-adjusted in sensorsReadTemperature
  const float setpointC = ACTIVE_PROFILE(runningCfg).setpoint;
  const float tempC = currentState.temperature;

  // Multi-level pulsing tuned for boiler thermal lag
  static uint32_t coolDownLockoutUntil = 0; // when > millis(), heater remains off to let system settle
  static float lastTempC = 0.0f;
  static uint32_t lastTempTs = 0;
  static uint32_t minOffHoldUntil = 0; // enforce brief off-hold near setpoint when rising fast

  // Time-proportional heater control with explicit duty and period
  auto timePropHeat = [](uint32_t periodMs, uint8_t dutyPct) {
    static uint32_t windowStart = 0;
    uint32_t now = millis();
    if (now - windowStart >= periodMs) {
      windowStart = now;
    }
    if (dutyPct == 0) {
      setBoilerOff();
      return;
    }
    if (dutyPct >= 100) {
      setBoilerOn();
      return;
    }
    uint32_t onTime = (periodMs * dutyPct) / 100u;
    if ((now - windowStart) < onTime) setBoilerOn(); else setBoilerOff();
  };

  // If we're above setpoint, ensure heater is off and optionally hold off briefly
  if (tempC > setpointC) {
    setBoilerOff();
    if (tempC >= setpointC + 0.5f) {
      coolDownLockoutUntil = millis() + 8000; // 8s lockout when > +0.5°C
    }
  }

  uint32_t nowTs = millis();
  // Compute temperature slope in °C/s (defensive against first run)
  float slopeCps = 0.0f;
  if (lastTempTs != 0) {
    float dt = (nowTs - lastTempTs) / 1000.0f;
    if (dt > 0.0f) slopeCps = (tempC - lastTempC) / dt;
  }
  lastTempC = tempC;
  lastTempTs = nowTs;

  if (nowTs < coolDownLockoutUntil) {
    setBoilerOff();
  } else {
    const float diff = setpointC - tempC; // positive when below target

    // Preemptive cut if rising fast near target; also enforce a minimum off hold
    if (diff <= 1.0f && slopeCps > 0.2f) {
      minOffHoldUntil = nowTs + 1500; // 1.5s off hold
    }
    if (nowTs < minOffHoldUntil) {
      setBoilerOff();
      return;
    }

    if (diff > 8.0f) {
      timePropHeat(1000, 100);
    } else if (diff > 4.0f) {
      timePropHeat(2000, 60);
    } else if (diff > 2.0f) {
      timePropHeat(3000, 35);
    } else if (diff > 1.0f) {
      // If rising fast, reduce or cut
      if (slopeCps > 0.25f) {
        setBoilerOff();
      } else {
        timePropHeat(4000, 18);
      }
    } else if (diff > 0.5f) {
      if (slopeCps > 0.20f) {
        setBoilerOff();
      } else {
        timePropHeat(5000, 10);
      }
    } else if (diff > 0.2f) {
      if (slopeCps > 0.15f) {
        setBoilerOff();
      } else {
        timePropHeat(6000, 5);
      }
    } else if (diff > 0.0f) {
      if (slopeCps > 0.10f) {
        setBoilerOff();
      } else {
        timePropHeat(7000, 3);
      }
    } else {
      setBoilerOff();
    }
  }
  
  // Valve control logic remains unchanged
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
