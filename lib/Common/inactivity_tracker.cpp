#include "inactivity_tracker.h"

InactivityTracker::InactivityTracker(uint32_t timeoutMs) : timeoutMs_(timeoutMs) {}

void InactivityTracker::reset(uint32_t nowMs) {
  lastActivityMs_ = nowMs;
  standby_ = false;
  initialized_ = true;
}

void InactivityTracker::noteActivity(uint32_t nowMs) {
  reset(nowMs);
}

void InactivityTracker::update(uint32_t nowMs, bool machineBusy) {
  if (!initialized_) {
    reset(nowMs);
    return;
  }

  if (machineBusy) {
    noteActivity(nowMs);
    return;
  }

  if ((nowMs - lastActivityMs_) >= timeoutMs_) {
    standby_ = true;
  }
}

bool InactivityTracker::isStandby() const {
  return standby_;
}
