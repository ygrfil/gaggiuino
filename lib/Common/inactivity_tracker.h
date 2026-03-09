#ifndef INACTIVITY_TRACKER_H
#define INACTIVITY_TRACKER_H

#include <stdint.h>

class InactivityTracker {
public:
  explicit InactivityTracker(uint32_t timeoutMs);

  void reset(uint32_t nowMs);
  void noteActivity(uint32_t nowMs);
  void update(uint32_t nowMs, bool machineBusy);
  bool isStandby() const;

private:
  uint32_t timeoutMs_;
  uint32_t lastActivityMs_ = 0;
  bool standby_ = false;
  bool initialized_ = false;
};

#endif
