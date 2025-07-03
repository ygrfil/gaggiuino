/* 09:32 15/03/2023 - change triggering comment */
#include "just_do_coffee.h"
#include "../lcd/lcd.h"

extern unsigned long steamTime;


void justDoCoffee(const eepromValues_t &runningCfg, const SensorState &currentState, const bool brewActive) {
  // Set target mode to brew temp
  lcdTargetState((int)HEATING::MODE_brew);
  
  float brewTempSetPoint = ACTIVE_PROFILE(runningCfg).setpoint + runningCfg.offsetTemp;
  float sensorTemperature = currentState.temperature + runningCfg.offsetTemp;

  // Control logic for brewing mode with ±1°C precision
  if (brewActive) {
    if(sensorTemperature <= brewTempSetPoint - 1.f) {
      setBoilerOn(); // Turn on when >1°C below target
    } else if (sensorTemperature >= brewTempSetPoint + 1.f) {
      setBoilerOff(); // Turn off when >1°C above target
    }
    // Within ±1°C band - maintain current heater state (no action)
  } else {
    // Idle mode - ±1°C precision
    if (sensorTemperature <= brewTempSetPoint - 1.f) {
      setBoilerOn(); // Turn on when >1°C below target
    } else if (sensorTemperature >= brewTempSetPoint + 1.f) {
      setBoilerOff(); // Turn off when >1°C above target  
    }
    // Within ±1°C band - maintain current heater state (no action)
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
