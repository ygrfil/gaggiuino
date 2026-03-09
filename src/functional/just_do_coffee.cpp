/* 09:32 15/03/2023 - change triggering comment */
#include "just_do_coffee.h"
#include "../lcd/lcd.h"

extern unsigned long steamTime;

void justDoCoffee(const eepromValues_t &runningCfg, const SensorState &currentState, const bool brewActive) {
  lcdTargetState((int)HEATING::MODE_brew); // setting the target mode to "brew temp"
  temperatureControlApplyBrew(runningCfg, currentState, brewActive);
  if (brewActive || !currentState.brewSwitchState) { // keep steam boiler supply valve open while steaming/descale only
    setSteamValveRelayOff();
  }
  setSteamBoilerRelayOff();
}

//#############################################################################################
//################################____STEAM_POWER_CONTROL____##################################
//#############################################################################################
void steamCtrl(const eepromValues_t &runningCfg, SensorState &currentState) {
  currentState.steamSwitchState ? lcdTargetState((int)HEATING::MODE_steam) : lcdTargetState((int)HEATING::MODE_brew); // setting the steam/hot water target temp
  if (currentState.temperature <= 0.f || currentState.temperature >= 170.f) {
    temperatureControlForceOff();
    setSteamBoilerRelayOff();
    setSteamValveRelayOff();
    setPumpOff();
    return;
  }
  if (currentState.smoothedPressure > steamThreshold_) {
    temperatureControlForceOff();
    setSteamBoilerRelayOff();
    setSteamValveRelayOff();
    setPumpOff();
  } else {
    temperatureControlApplySteam(runningCfg, currentState);
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
  temperatureControlReset();
  closeValve();
  setPumpToRawValue(80);
  if (currentState.temperature < MAX_WATER_TEMP) {
    setBoilerOn();
  } else {
    setBoilerOff();
  }
}
