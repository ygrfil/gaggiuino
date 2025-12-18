/* 09:32 15/03/2023 - change triggering comment */
#ifndef PREDICTIVE_WEIGHT_H
#define PREDICTIVE_WEIGHT_H

#include "profiling_phases.h"
#include "sensors_state.h"
#include "../eeprom_data/eeprom_data.h"

extern int preInfusionFinishedPhaseIdx;
constexpr float crossSectionalArea = 0.0026f; // avg puck crossectional area.
constexpr float dynamicViscosity = 0.0002964f; // avg water dynamic viscosity at 90-95 celsius.
bool predictiveTargetReached = false;
int predictivePreinfusionFinishedCheck = 0.f;

class PredictiveWeight {
private:
  bool outputFlowStarted;
  bool isForceStarted;
  float puckResistance;
  float truePuckResistance;
  float resistanceDelta;
  float pressureDrop;

public:
  bool preinfusionFinished;
  PredictiveWeight() :
    outputFlowStarted(false),
    isForceStarted(false),
    puckResistance(0.f),
    truePuckResistance(0.f),
    resistanceDelta(0.f),
    pressureDrop(0.f),
    preinfusionFinished(false)
  {}

  bool isOutputFlow() {
    return outputFlowStarted;
  }

  inline float calculatePuckResistance(float waterFlowRate, float crossSectionalArea, float dynamicViscosity, float pressureDrop) {
    float resistance = -(dynamicViscosity * waterFlowRate) / (crossSectionalArea * pressureDrop);
    return resistance;
  }

  void update(const SensorState& state, CurrentPhase& phase, const eepromValues_t& cfg) {
    // Early exit if already started
    if (isForceStarted || outputFlowStarted || state.waterPumped >= 18.f) {
      outputFlowStarted = true;
      return;
    }

    // Update resistance calculations
    float previousPuckResistance = puckResistance;
    puckResistance = state.smoothedPressure * 1000.f / max(state.smoothedPumpFlow, 0.001f);
    resistanceDelta = (puckResistance - previousPuckResistance) * 0.8f + resistanceDelta * 0.2f;

    pressureDrop = fmaxf(state.smoothedPressure * 10.f - state.pumpClicks * 0.8f, 1.f);
    truePuckResistance = calculatePuckResistance(state.smoothedPumpFlow, crossSectionalArea, dynamicViscosity, pressureDrop);

    // Determine phase and profile state
    bool phaseTypePressure = (phase.getType() == PHASE_TYPE::PHASE_TYPE_PRESSURE);
    predictivePreinfusionFinishedCheck = phase.getIndex();
    
    int finishIdx = (ACTIVE_PROFILE(cfg).preinfusionState && ACTIVE_PROFILE(cfg).soakState) 
                    ? preInfusionFinishedPhaseIdx - 1 
                    : preInfusionFinishedPhaseIdx;
    preinfusionFinished = (predictivePreinfusionFinishedCheck >= finishIdx);

    // Check soak state
    bool soakEnabled = ACTIVE_PROFILE(cfg).soakState && 
                       (phaseTypePressure ? ACTIVE_PROFILE(cfg).soakTimePressure > 0 
                                          : ACTIVE_PROFILE(cfg).soakTimeFlow > 0);
    
    float pressureTarget = phaseTypePressure ? ACTIVE_PROFILE(cfg).preinfusionBar 
                                             : ACTIVE_PROFILE(cfg).preinfusionFlowPressureTarget;

    // Handle soak phase
    if (!preinfusionFinished && soakEnabled) {
      if (!predictiveTargetReached) {
        if (state.smoothedPressure >= pressureTarget) predictiveTargetReached = true;
        return;
      }
      // Check pressure drop after target reached
      if (pressureTarget - state.smoothedPressure > 1.f) outputFlowStarted = true;
      return;
    }

    // Minimum pressure threshold for output detection
    if (state.smoothedPressure < 1.2f) return;

    // Pressure-phase specific checks
    if (phaseTypePressure) {
      if (fabsf(state.pressureChangeSpeed) > 5.f || fabsf(state.pumpFlowChangeSpeed) > 2.f) return;
      if (resistanceDelta > 800.f || puckResistance < 800.f) return;
    }

    // Final puck resistance check
    if (truePuckResistance < -0.025f) return;

    outputFlowStarted = true;
  }

  void setIsForceStarted(bool value) {
    isForceStarted = value;
  }

  void reset() {
    puckResistance = 0.f;
    resistanceDelta = 0.f;
    isForceStarted = false;
    outputFlowStarted = false;
    predictiveTargetReached = false;
    preinfusionFinished = false;
    truePuckResistance = 0.f;
    pressureDrop = 0.f;
  }
};

#endif
