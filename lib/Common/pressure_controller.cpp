#include "pressure_controller.h"

namespace {
float clampf(const float value, const float minValue, const float maxValue) {
  return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}
}

PressureController::PressureController(const PressureControllerConfig& config) : config_(config) {}

void PressureController::configure(const PressureControllerConfig& config) {
  config_ = config;
}

void PressureController::reset(void) {
  integral_ = 0.f;
  lastOutput_ = 0.f;
  lastUpdateMs_ = 0u;
  initialized_ = false;
}

float PressureController::update(const PressureControllerInput& input, uint32_t nowMs) {
  if (input.targetPressure <= 0.f || input.maxPumpPct <= 0.f) {
    reset();
    return 0.f;
  }

  const float maxPumpPct = clampf(input.maxPumpPct, config_.outputMin, config_.outputMax);
  const float basePumpPct = clampf(input.basePumpPct, config_.outputMin, maxPumpPct);
  const float error = input.targetPressure - input.currentPressure;

  if (!initialized_) {
    initialized_ = true;
    lastUpdateMs_ = nowMs;
    lastOutput_ = clampf(
      input.currentPressure < config_.startupThreshold ? config_.startupFloor : basePumpPct,
      config_.outputMin,
      maxPumpPct
    );
    return lastOutput_;
  }

  const uint32_t elapsedMs = nowMs - lastUpdateMs_;
  if (elapsedMs == 0u) {
    return lastOutput_;
  }

  const float dt = static_cast<float>(elapsedMs) / 1000.f;
  const float proposedIntegral = clampf(
    integral_ + (config_.ki * error * dt),
    config_.integralMin,
    config_.integralMax
  );
  float rawOutput = basePumpPct + (config_.kp * error) + proposedIntegral;

  if (input.currentPressure < config_.startupThreshold && error > 0.4f) {
    rawOutput = rawOutput < config_.startupFloor ? config_.startupFloor : rawOutput;
  }

  if (input.pressureChangeSpeed > 0.25f && error < 0.15f) {
    rawOutput = rawOutput > lastOutput_ ? lastOutput_ : rawOutput;
  }

  float clampedOutput = clampf(rawOutput, config_.outputMin, maxPumpPct);
  const bool saturatedHigh = rawOutput > maxPumpPct;
  const bool saturatedLow = rawOutput < config_.outputMin;
  const bool antiWindupAllowsUpdate =
    (!saturatedHigh && !saturatedLow) ||
    (saturatedHigh && error < 0.f) ||
    (saturatedLow && error > 0.f);

  if (antiWindupAllowsUpdate) {
    integral_ = proposedIntegral;
  }

  const float maxStep = config_.slewRatePerSecond * dt;
  if (clampedOutput > lastOutput_ + maxStep) {
    clampedOutput = lastOutput_ + maxStep;
  } else if (clampedOutput < lastOutput_ - maxStep) {
    clampedOutput = lastOutput_ - maxStep;
  }

  lastOutput_ = clampf(clampedOutput, config_.outputMin, maxPumpPct);
  lastUpdateMs_ = nowMs;
  return lastOutput_;
}
