#ifndef PRESSURE_CONTROLLER_H
#define PRESSURE_CONTROLLER_H

#include <stdint.h>

struct PressureControllerConfig {
  float kp = 0.10f;
  float ki = 0.05f;
  float outputMin = 0.f;
  float outputMax = 1.f;
  float integralMin = -0.35f;
  float integralMax = 0.35f;
  float slewRatePerSecond = 1.4f;
  float startupFloor = 0.22f;
  float startupThreshold = 1.2f;
};

struct PressureControllerInput {
  float targetPressure = 0.f;
  float currentPressure = 0.f;
  float pressureChangeSpeed = 0.f;
  float basePumpPct = 0.f;
  float maxPumpPct = 1.f;
};

class PressureController {
public:
  PressureController() = default;
  explicit PressureController(const PressureControllerConfig& config);

  void configure(const PressureControllerConfig& config);
  void reset(void);
  float update(const PressureControllerInput& input, uint32_t nowMs);

private:
  PressureControllerConfig config_;
  float integral_ = 0.f;
  float lastOutput_ = 0.f;
  uint32_t lastUpdateMs_ = 0;
  bool initialized_ = false;
};

#endif
