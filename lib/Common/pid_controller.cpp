#include "pid_controller.h"

#include <math.h>

namespace {
float clampf(const float value, const float minValue, const float maxValue) {
  return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}
}

PIDController::PIDController(const PIDConfig& config) : config_(config) {}

void PIDController::configure(const PIDConfig& config) {
  config_ = config;
}

void PIDController::reset(float measurement) {
  integral_ = 0.f;
  filteredDerivative_ = 0.f;
  lastMeasurement_ = measurement;
  lastOutput_ = 0.f;
  lastUpdateMs_ = 0;
  initialized_ = false;
}

float PIDController::update(float setpoint, float measurement, uint32_t nowMs) {
  const float error = setpoint - measurement;

  if (!initialized_) {
    initialized_ = true;
    lastMeasurement_ = measurement;
    lastUpdateMs_ = nowMs;
    lastOutput_ = clampf(config_.kp * error, config_.outputMin, config_.outputMax);
    return lastOutput_;
  }

  const uint32_t elapsedMs = nowMs - lastUpdateMs_;
  if (elapsedMs == 0u) {
    return lastOutput_;
  }

  const float dt = static_cast<float>(elapsedMs) / 1000.f;
  const float measurementDelta = measurement - lastMeasurement_;
  const float derivative = -measurementDelta / dt;
  filteredDerivative_ = (config_.derivativeAlpha * filteredDerivative_) +
    ((1.f - config_.derivativeAlpha) * derivative);

  const float proposedIntegral = clampf(
    integral_ + (config_.ki * error * dt),
    config_.integralMin,
    config_.integralMax
  );
  const float proportional = config_.kp * error;
  const float derivativeTerm = config_.kd * filteredDerivative_;

  float unclampedOutput = proportional + proposedIntegral + derivativeTerm;
  float clampedOutput = clampf(unclampedOutput, config_.outputMin, config_.outputMax);

  const bool saturatedHigh = unclampedOutput > config_.outputMax;
  const bool saturatedLow = unclampedOutput < config_.outputMin;
  const bool antiWindupAllowsUpdate =
    (!saturatedHigh && !saturatedLow) ||
    (saturatedHigh && error < 0.f) ||
    (saturatedLow && error > 0.f);

  if (antiWindupAllowsUpdate) {
    integral_ = proposedIntegral;
    unclampedOutput = proportional + integral_ + derivativeTerm;
    clampedOutput = clampf(unclampedOutput, config_.outputMin, config_.outputMax);
  }

  lastMeasurement_ = measurement;
  lastUpdateMs_ = nowMs;
  lastOutput_ = clampedOutput;
  return lastOutput_;
}

float PIDController::lastOutput() const {
  return lastOutput_;
}
