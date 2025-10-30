/* 09:32 15/03/2023 - change triggering comment */
#include "profiling_phases.h"

//----------------------------------------------------------------------//
//------------------------- ShotSnapshot -------------------------------//
//----------------------------------------------------------------------//

ShotSnapshot buildShotSnapshot(uint32_t timeInShot, const SensorState& state, CurrentPhase& phase) {
  float targetFlow = (phase.getType() == PHASE_TYPE::PHASE_TYPE_FLOW) ? phase.getTarget() : phase.getRestriction();
  float targetPressure = (phase.getType() == PHASE_TYPE::PHASE_TYPE_PRESSURE) ? phase.getTarget() : phase.getRestriction();

  return ShotSnapshot{
    .timeInShot = timeInShot,
    .pressure = state.smoothedPressure,
    .pumpFlow = state.smoothedPumpFlow,
    .weightFlow = state.smoothedWeightFlow,
    .temperature = state.waterTemperature,
    .shotWeight = state.shotWeight,
    .waterPumped = state.waterPumped,
    .targetTemperature = -1,
    .targetPumpFlow = targetFlow,
    .targetPressure = targetPressure
  };
};

//----------------------------------------------------------------------//
//------------------------------ Phase ---------------------------------//
//----------------------------------------------------------------------//
float Phase::getTarget(uint32_t timeInPhase, const ShotSnapshot& stateAtStart) const {
  long transitionTime = fmax(0L, target.time);
  float startValue = target.start > 0.f
    ? target.start
    : type == PHASE_TYPE::PHASE_TYPE_FLOW ? stateAtStart.pumpFlow : stateAtStart.pressure;

  return mapRange(timeInPhase, 0.f, transitionTime, startValue, target.end, 1, target.curve);
}

float Phase::getRestriction() const {
  return restriction;
}

bool Phase::isStopConditionReached(SensorState& currentState, uint32_t timeInShot, ShotSnapshot stateAtPhaseStart) const {
  return stopConditions.isReached(currentState, timeInShot, stateAtPhaseStart);
}

//----------------------------------------------------------------------//
//-------------------------- StopConditions ----------------------------//
//----------------------------------------------------------------------//
/**
  * The method below predicts if we should already consider the condition achieved when we have a slow reaction time
  */
inline bool predictTargerAchieved(const float targetValue, const float currentValue, const float changeSpeed, const float reactionTime = 0.f) {
  // If no change happening, check if we're already at target
  if (changeSpeed == 0.f) {
    return currentValue >= targetValue;
  }

  float remainingDose = targetValue - currentValue;
  
  // CRITICAL FIX: If flow is negative (weight decreasing), we're moving AWAY from target, not towards it
  // This prevents false "target achieved" when scales have negative readings or noise
  if (changeSpeed < 0.f) {
    return false; // Can't reach target with negative flow
  }
  
  // Only predict target achieved if we're moving in the right direction (positive flow towards target)
  float secondsRemaining = remainingDose / changeSpeed;

  return secondsRemaining < reactionTime ? true : false;
}

bool PhaseStopConditions::isReached(SensorState& state, long timeInShot, ShotSnapshot stateAtPhaseStart) const {
  auto stopOn = this;
  uint32_t timeInPhase = timeInShot - stateAtPhaseStart.timeInShot;
  float flow = state.weight > 0.4f ? state.smoothedWeightFlow : state.smoothedPumpFlow;
  float currentWaterPumpedInPhase = state.waterPumped - stateAtPhaseStart.waterPumped;

  return (stopOn->time >= 0L && timeInPhase >= static_cast<uint32_t>(stopOn->time)) ||
    (stopOn->weight > 0.f && state.shotWeight > stopOn->weight) ||
    (stopOn->pressureAbove > 0.f && state.smoothedPressure > stopOn->pressureAbove) ||
    (stopOn->pressureBelow > 0.f && state.smoothedPressure < stopOn->pressureBelow) ||
    (stopOn->waterPumpedInPhase > 0.f && currentWaterPumpedInPhase >= stopOn->waterPumpedInPhase) ||
    (stopOn->flowAbove > 0.f && state.smoothedPumpFlow > stopOn->flowAbove) ||
    (stopOn->flowBelow > 0.f && state.smoothedPumpFlow < stopOn->flowBelow);
}

bool GlobalStopConditions::isReached(const SensorState& state, uint32_t timeInShot) {
  // CRITICAL FIX: Prevent false triggers from invalid timeInShot values
  // If timeInShot is suspiciously large (> 2 hours), it's likely a calculation error
  // This can happen if brewingTimer wasn't reset properly or due to millis() overflow issues
  const uint32_t MAX_REASONABLE_SHOT_TIME = 7200000; // 2 hours in milliseconds
  
  if (timeInShot < 1000) { // No shot lasts less than 1 second
    return false;
  }
  
  if (timeInShot > MAX_REASONABLE_SHOT_TIME) {
    // Invalid timeInShot - likely a bug, don't trigger stop condition
    return false;
  }

  auto stopOn = this;
  float flow = state.weight > 0.4f ? state.smoothedWeightFlow : state.smoothedPumpFlow;

  return (stopOn->weight > 0.f && predictTargerAchieved(stopOn->weight, state.shotWeight, flow, 0.5f)) ||
    (stopOn->waterPumped > 0.f && state.waterPumped > stopOn->waterPumped) ||
    (stopOn->time > 0L && timeInShot >= stopOn->time);
}

//----------------------------------------------------------------------//
//--------------------------- CurrentPhase -----------------------------//
//----------------------------------------------------------------------//
CurrentPhase::CurrentPhase() : index(0), phase(nullptr), timeInPhase(0), shotSnapshotAtStart(nullptr) {}
CurrentPhase::CurrentPhase(int index, const Phase& phase, uint32_t timeInPhase, const ShotSnapshot& shotSnapshotAtStart) : index(index), phase{ &phase }, timeInPhase(timeInPhase), shotSnapshotAtStart{ &shotSnapshotAtStart } {}
CurrentPhase::CurrentPhase(const CurrentPhase& currentPhase) : index(currentPhase.index), phase{ currentPhase.phase }, timeInPhase(currentPhase.timeInPhase), shotSnapshotAtStart{ currentPhase.shotSnapshotAtStart } {}

Phase CurrentPhase::getPhase() { 
  static Phase emptyPhase;
  return phase ? *phase : emptyPhase;
}

PHASE_TYPE CurrentPhase::getType() { 
  return phase ? phase->type : PHASE_TYPE::PHASE_TYPE_FLOW;
}

int CurrentPhase::getIndex() { return index; }

long CurrentPhase::getTimeInPhase() { return timeInPhase; }

float CurrentPhase::getTarget() { 
  if (!phase) return 0.f;
  
  // Use stored snapshot if available, otherwise use empty snapshot for initialization
  static ShotSnapshot emptySnapshot = {0, 0, 0, 0, 0, 0, 0};
  const ShotSnapshot& snapshot = shotSnapshotAtStart ? *shotSnapshotAtStart : emptySnapshot;
  
  return phase->getTarget(timeInPhase, snapshot);
}

float CurrentPhase::getRestriction() { 
  return phase ? phase->getRestriction() : 0.f;
}

void CurrentPhase::update(int index, Phase& phase, uint32_t timeInPhase) {
  CurrentPhase::index = index;
  CurrentPhase::phase = &phase;
  CurrentPhase::timeInPhase = timeInPhase;
  // Note: shotSnapshotAtStart is NOT updated, will use default empty snapshot in getTarget()
}

void CurrentPhase::update(int index, Phase& phase, uint32_t timeInPhase, const ShotSnapshot& snapshot) {
  CurrentPhase::index = index;
  CurrentPhase::phase = &phase;
  CurrentPhase::timeInPhase = timeInPhase;
  CurrentPhase::shotSnapshotAtStart = &snapshot;
}

//----------------------------------------------------------------------//
//-------------------------- PhaseProfiler -----------------------------//
//----------------------------------------------------------------------//

PhaseProfiler::PhaseProfiler(Profile& profile) : profile(profile) {
  // Initialize currentPhase safely only if profile has phases
  if (profile.phaseCount() > 0) {
    currentPhase.update(0, profile.phases[0], 0);
  }
}

void PhaseProfiler::updatePhase(uint32_t timeInShot, SensorState& state) {
  // CRITICAL FIX: Safety check for invalid profile state
  // If profile is empty, mark as finished immediately to prevent undefined behavior
  if (profile.phaseCount() == 0) {
    currentPhaseIdx = 0;
    return;
  }
  
  size_t phaseIdx = currentPhaseIdx;
  
  // CRITICAL FIX: Prevent underflow in timeInPhase calculation
  // If phaseChangedSnapshot.timeInShot is larger than timeInShot (shouldn't happen but could due to timing issues),
  // clamp timeInPhase to 0 instead of allowing unsigned wrap-around
  uint32_t timeInPhase = (timeInShot >= phaseChangedSnapshot.timeInShot) 
    ? (timeInShot - phaseChangedSnapshot.timeInShot) 
    : 0;

  if (phaseIdx >= profile.phaseCount() || profile.globalStopConditions.isReached(state, timeInShot)) {
    currentPhaseIdx = profile.phaseCount();
    // Fix: Use last valid phase index instead of out-of-bounds phaseIdx
    size_t lastPhaseIdx = profile.phaseCount() > 0 ? profile.phaseCount() - 1 : 0;
    if (profile.phaseCount() > 0) {
      currentPhase.update(lastPhaseIdx, profile.phases[lastPhaseIdx], timeInPhase, phaseChangedSnapshot);
    }
    return;
  }

  if (!profile.phases[phaseIdx].isStopConditionReached(state, timeInShot, phaseChangedSnapshot)) {
    currentPhase.update(phaseIdx, profile.phases[phaseIdx], timeInPhase, phaseChangedSnapshot);
    return;
  }

  currentPhase.update(phaseIdx, profile.phases[phaseIdx], timeInPhase, phaseChangedSnapshot);
  phaseChangedSnapshot = buildShotSnapshot(timeInShot, state, currentPhase);
  currentPhaseIdx += 1;
  updatePhase(timeInShot, state);
}

// Gets the profiling phase we should be in based on the timeInShot and the Sensors state
CurrentPhase& PhaseProfiler::getCurrentPhase() {
  return currentPhase;
}

bool PhaseProfiler::isFinished() {
  return currentPhaseIdx >= profile.phaseCount();
}

void PhaseProfiler::reset() {
  currentPhaseIdx = 0;
  phaseChangedSnapshot = ShotSnapshot{};
  // Safety check: only update currentPhase if profile has phases
  if (profile.phaseCount() > 0) {
    currentPhase.update(0, profile.phases[0], 0);
  }
}
