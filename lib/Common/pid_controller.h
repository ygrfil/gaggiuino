#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <stdint.h>

struct PIDConfig {
  float kp = 0.f;
  float ki = 0.f;
  float kd = 0.f;
  float outputMin = 0.f;
  float outputMax = 100.f;
  float integralMin = -100.f;
  float integralMax = 100.f;
  float derivativeAlpha = 0.2f;
};

class PIDController {
public:
  PIDController() = default;
  explicit PIDController(const PIDConfig& config);

  void configure(const PIDConfig& config);
  void reset(float measurement = 0.f);
  float update(float setpoint, float measurement, uint32_t nowMs);

  float lastOutput() const;

private:
  PIDConfig config_;
  float integral_ = 0.f;
  float filteredDerivative_ = 0.f;
  float lastMeasurement_ = 0.f;
  float lastOutput_ = 0.f;
  uint32_t lastUpdateMs_ = 0;
  bool initialized_ = false;
};

#endif
