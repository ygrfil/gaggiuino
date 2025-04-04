/* 09:32 15/03/2023 - change triggering comment */
#include "just_do_coffee.h"
#include "../lcd/lcd.h"

extern unsigned long steamTime;

// Optimized TEMP_DELTA calculation using integer math when possible
inline static int16_t TEMP_DELTA_OPTIMIZED(int16_t tempSetpoint, const SensorState &currentState) {
  // Use integer scaling factor (100x) to avoid floating point
  int16_t pumpFlowScaled = currentState.pumpFlow * 100;
  int16_t divisor = (pumpFlowScaled < 100) ? 700 : 500; // <1.0 ? 7.0 : 5.0
  return (tempSetpoint * pumpFlowScaled) / divisor;
}

void justDoCoffee(const eepromValues_t &runningCfg, const SensorState &currentState, const bool brewActive) {
  // Set target mode to brew temp
  lcdTargetState((int)HEATING::MODE_brew);
  
  // Scale temperatures by 10 to use integer math (higher precision)
  const int16_t TEMP_SCALE = 10;
  int16_t brewTempSetPoint = (ACTIVE_PROFILE(runningCfg).setpoint + runningCfg.offsetTemp) * TEMP_SCALE;
  int16_t sensorTemperature = (currentState.temperature + runningCfg.offsetTemp) * TEMP_SCALE;
  
  // Threshold temperatures (scaled by 10)
  const int16_t BREW_TEMP_SAFETY_MARGIN = 50; // 5.0 degrees
  const int16_t IDLE_TEMP_LOWER_THRESHOLD = 100; // 10.0 degrees
  
  // Control logic for brewing mode
  if (brewActive) {
    // Brewing mode
    if(sensorTemperature <= brewTempSetPoint - BREW_TEMP_SAFETY_MARGIN) {
      // Temperature too low - turn on boiler at full power
      setBoilerOn();
    } else {
      // Near target temperature - use PWM control
      int16_t deltaOffset = 0;
      
      // Apply delta temperature compensation if enabled
      if (runningCfg.brewDeltaState) {
        int16_t tempDelta = TEMP_DELTA_OPTIMIZED(brewTempSetPoint / TEMP_SCALE, currentState);
        // Simplified delta calculation
        if (sensorTemperature > brewTempSetPoint) {
          // At or above target - no compensation needed
          deltaOffset = 0;
        } else if (sensorTemperature <= brewTempSetPoint) {
          // Scale compensation based on temperature difference
          const int16_t tempRange = tempDelta * TEMP_SCALE;
          deltaOffset = ((brewTempSetPoint - sensorTemperature) * tempDelta) / tempRange;
          // Constrain delta offset
          if (deltaOffset > tempDelta) deltaOffset = tempDelta;
          if (deltaOffset < 0) deltaOffset = 0;
        }
      }
      
      // Apply heat if needed
      if (sensorTemperature <= brewTempSetPoint + deltaOffset) {
        pulseHeaters(runningCfg.hpwr, runningCfg.mainDivider, runningCfg.brewDivider, brewActive);
      } else {
        setBoilerOff();
      }
    }
  } else {
    // Idle mode - simpler logic
    if (sensorTemperature <= brewTempSetPoint - IDLE_TEMP_LOWER_THRESHOLD) {
      // Cold - full power
      setBoilerOn();
    } else {
      // Calculate appropriate power level
      int HPWR_LOW = runningCfg.hpwr / runningCfg.mainDivider;
      int heatPower;
      
      // Simplified power calculation based on temperature range
      if (sensorTemperature <= brewTempSetPoint - BREW_TEMP_SAFETY_MARGIN) {
        // Between 5-10 degrees below target - medium power
        heatPower = runningCfg.hpwr / 2 + HPWR_LOW / 2; // Average of max and min
      } else if (sensorTemperature < brewTempSetPoint) {
        // Less than 5 degrees below target - low power
        heatPower = HPWR_LOW;
      } else {
        // At or above target - no heat
        setBoilerOff();
        heatPower = 0; // Not actually used but set for clarity
      }
      
      // Apply heat if needed
      if (sensorTemperature < brewTempSetPoint) {
        // Use appropriate pulse pattern based on temperature
        if (sensorTemperature <= brewTempSetPoint - BREW_TEMP_SAFETY_MARGIN) {
          pulseHeaters(heatPower, 1, runningCfg.mainDivider, brewActive);
        } else {
          pulseHeaters(heatPower, runningCfg.brewDivider, runningCfg.brewDivider, brewActive);
        }
      } else {
        setBoilerOff();
      }
    }
  }
  
  // Valve control logic remains unchanged
  if (brewActive || !currentState.brewSwitchState) {
    setSteamValveRelayOff();
  }
  setSteamBoilerRelayOff();
}

void pulseHeaters(const uint32_t pulseLength, const int factor_1, const int factor_2, const bool brewActive) {
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
}

//#############################################################################################
//################################____STEAM_POWER_CONTROL____##################################
//#############################################################################################
void steamCtrl(const eepromValues_t &runningCfg, SensorState &currentState) {
  currentState.steamSwitchState ? lcdTargetState((int)HEATING::MODE_steam) : lcdTargetState((int)HEATING::MODE_brew); // setting the steam/hot water target temp
  // steam temp control, needs to be aggressive to keep steam pressure acceptable
  float steamTempSetPoint = runningCfg.steamSetPoint + runningCfg.offsetTemp;
  float sensorTemperature = currentState.temperature + runningCfg.offsetTemp;

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
