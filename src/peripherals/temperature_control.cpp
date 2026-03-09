#include "temperature_control.h"

#include <Arduino.h>

#include "pid_controller.h"
#include "peripherals.h"

namespace {
constexpr uint32_t HEATER_WINDOW_MS = 1000UL;
constexpr float BREW_FULL_HEAT_DELTA = 6.f;
constexpr float BREW_CUTOFF_DELTA = 0.8f;
constexpr float STEAM_FULL_HEAT_DELTA = 10.f;
constexpr float STEAM_CUTOFF_DELTA = 1.5f;
constexpr float STEAM_PRESSURE_LIMIT = 11.f;

enum class TemperatureMode {
  IDLE,
  BREW,
  STEAM
};

PIDController brewController(PIDConfig{
  .kp = 12.0f,
  .ki = 0.28f,
  .kd = 3.5f,
  .outputMin = 0.f,
  .outputMax = 100.f,
  .integralMin = -35.f,
  .integralMax = 35.f,
  .derivativeAlpha = 0.35f
});

PIDController steamController(PIDConfig{
  .kp = 9.0f,
  .ki = 0.35f,
  .kd = 1.5f,
  .outputMin = 0.f,
  .outputMax = 100.f,
  .integralMin = -30.f,
  .integralMax = 30.f,
  .derivativeAlpha = 0.25f
});

TemperatureMode activeMode = TemperatureMode::IDLE;
uint32_t heaterWindowStart = 0;
bool standbyEnabled = false;

bool isTemperatureValid(const float temperature) {
  return temperature > 0.f && temperature < 170.f;
}

void resetForMode(TemperatureMode newMode, float measurement) {
  if (activeMode == newMode) {
    return;
  }

  brewController.reset(measurement);
  steamController.reset(measurement);
  heaterWindowStart = millis();
  activeMode = newMode;
}

void applyDutyCycle(const float dutyCycle, const uint32_t nowMs) {
  if (heaterWindowStart == 0u || (nowMs - heaterWindowStart) >= HEATER_WINDOW_MS) {
    heaterWindowStart = nowMs;
  }

  const float boundedDutyCycle = dutyCycle < 0.f ? 0.f : (dutyCycle > 100.f ? 100.f : dutyCycle);
  const float onTime = (boundedDutyCycle / 100.f) * static_cast<float>(HEATER_WINDOW_MS);

  if (static_cast<float>(nowMs - heaterWindowStart) < onTime) {
    setBoilerOn();
  } else {
    setBoilerOff();
  }
}
}

void temperatureControlInit(void) {
  heaterWindowStart = millis();
  activeMode = TemperatureMode::IDLE;
  standbyEnabled = false;
}

void temperatureControlReset(void) {
  brewController.reset();
  steamController.reset();
  heaterWindowStart = millis();
  activeMode = TemperatureMode::IDLE;
}

void temperatureControlSetStandby(bool enabled) {
  standbyEnabled = enabled;
  if (enabled) {
    temperatureControlForceOff();
  }
}

void temperatureControlForceOff(void) {
  setBoilerOff();
  brewController.reset();
  steamController.reset();
  heaterWindowStart = millis();
  activeMode = TemperatureMode::IDLE;
}

void temperatureControlApplyBrew(const eepromValues_t& runningCfg, const SensorState& currentState, const bool brewActive) {
  const uint32_t nowMs = millis();
  const float setpoint = ACTIVE_PROFILE(runningCfg).setpoint + runningCfg.offsetTemp;
  const float sensorTemperature = currentState.temperature + runningCfg.offsetTemp;

  if (standbyEnabled || !isTemperatureValid(sensorTemperature)) {
    temperatureControlForceOff();
    return;
  }

  resetForMode(TemperatureMode::BREW, sensorTemperature);

  float dutyCycle = 0.f;
  const float fullHeatDelta = brewActive ? BREW_FULL_HEAT_DELTA * 0.5f : BREW_FULL_HEAT_DELTA;

  if (sensorTemperature <= setpoint - fullHeatDelta) {
    dutyCycle = 100.f;
  } else if (sensorTemperature >= setpoint + BREW_CUTOFF_DELTA) {
    brewController.reset(sensorTemperature);
    dutyCycle = 0.f;
  } else {
    dutyCycle = brewController.update(setpoint, sensorTemperature, nowMs);
    if (!brewActive) {
      dutyCycle *= 0.85f;
    }
  }

  applyDutyCycle(dutyCycle, nowMs);
}

void temperatureControlApplySteam(const eepromValues_t& runningCfg, SensorState& currentState) {
  const uint32_t nowMs = millis();
  const float setpoint = runningCfg.steamSetPoint + runningCfg.offsetTemp;
  const float sensorTemperature = currentState.temperature + runningCfg.offsetTemp;

  if (standbyEnabled || !isTemperatureValid(sensorTemperature) || currentState.smoothedPressure > STEAM_PRESSURE_LIMIT) {
    temperatureControlForceOff();
    return;
  }

  resetForMode(TemperatureMode::STEAM, sensorTemperature);

  float dutyCycle = 0.f;
  if (sensorTemperature <= setpoint - STEAM_FULL_HEAT_DELTA) {
    dutyCycle = 100.f;
  } else if (sensorTemperature >= setpoint + STEAM_CUTOFF_DELTA) {
    steamController.reset(sensorTemperature);
    dutyCycle = 0.f;
  } else {
    dutyCycle = steamController.update(setpoint, sensorTemperature, nowMs);
  }

  applyDutyCycle(dutyCycle, nowMs);
}
