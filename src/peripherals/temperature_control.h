#ifndef TEMPERATURE_CONTROL_H
#define TEMPERATURE_CONTROL_H

#include "eeprom_data/eeprom_data.h"
#include "sensors_state.h"

void temperatureControlInit(void);
void temperatureControlReset(void);
void temperatureControlSetStandby(bool enabled);
void temperatureControlForceOff(void);
void temperatureControlApplyBrew(const eepromValues_t& runningCfg, const SensorState& currentState, bool brewActive);
void temperatureControlApplySteam(const eepromValues_t& runningCfg, SensorState& currentState);

#endif
