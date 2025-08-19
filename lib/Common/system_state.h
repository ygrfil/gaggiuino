/* 09:32 15/03/2023 - change triggering comment */
#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

struct SystemState {
  bool startupInitFinished;
  bool autoShutdownEnabled;  // Enable/disable auto-shutdown feature
  bool shutdownWarningShown; // Track if warning has been shown
  bool shutdownActive;       // System is in shutdown mode (non-blocking state)
};

#endif
