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
    pressureDrop(0.f)
  {}

  bool isOutputFlow() {
    return outputFlowStarted;
  }

  inline float calculatePuckResistance(float waterFlowRate, float crossSectionalArea, float dynamicViscosity, float pressureDrop) {
    float resistance = -(dynamicViscosity * waterFlowRate) / (crossSectionalArea * pressureDrop);
    return resistance;
  }

  void update(const SensorState& state, CurrentPhase& phase, const eepromValues_t& cfg) {
    // Force predictive output after reasonable volume pumped
    if (isForceStarted || outputFlowStarted || state.waterPumped >= 50.f) { // Reduced from 65ml to 50ml
      outputFlowStarted = true;
      return;
    }
    
    float previousPuckResistance = puckResistance;
    // Improved resistance calculation with dampening for stability
    puckResistance = (state.smoothedPressure * 1000.f / max(state.smoothedPumpFlow, 0.001f)); 
    
    // Apply exponential smoothing to resistance changes
    resistanceDelta = (puckResistance - previousPuckResistance) * 0.8f + resistanceDelta * 0.2f;
    
    // More responsive pressure drop detection
    pressureDrop = state.smoothedPressure * 10.f - state.pumpClicks * 0.8f;
    pressureDrop = pressureDrop > 0.f ? pressureDrop : 1.f;
    
    truePuckResistance = calculatePuckResistance(
      state.smoothedPumpFlow, 
      crossSectionalArea, 
      dynamicViscosity, 
      pressureDrop
    );
    
    // Early exit for established flow - removed to avoid conflict with line 111
    // The main threshold check happens later in the function
    
    /* ::OBSERVATIONS::
    Through empirical testing it's been observed that ~2 bars is the indicator of the pf headspace being full
    as well as there being enough pressure for water to wet the puck enough to start the output.
    On profiles whare pressure drop is of concern ~1 bar of drop is the point where liquid output starts. */

    bool phaseTypePressure = phase.getType() == PHASE_TYPE::PHASE_TYPE_PRESSURE;
    predictivePreinfusionFinishedCheck = phase.getIndex();
    preinfusionFinished = ACTIVE_PROFILE(cfg).preinfusionState && ACTIVE_PROFILE(cfg).soakState
                              ? predictivePreinfusionFinishedCheck >= preInfusionFinishedPhaseIdx-1
                              : predictivePreinfusionFinishedCheck >= preInfusionFinishedPhaseIdx;

    bool soakEnabled = false;
    soakEnabled = ACTIVE_PROFILE(cfg).soakState
                    ? phaseTypePressure
                      ? ACTIVE_PROFILE(cfg).soakTimePressure > 0
                      : ACTIVE_PROFILE(cfg).soakTimeFlow > 0
                    : false;
    float pressureTarget = phaseTypePressure ? ACTIVE_PROFILE(cfg).preinfusionBar : ACTIVE_PROFILE(cfg).preinfusionFlowPressureTarget;


    // Pressure has to reach full pi target bar threshold.
    if (!preinfusionFinished  && soakEnabled) {
      if (predictiveTargetReached) {
        // pressure drop needs to be around 1.5bar since target hit for output flow to be considered started.
        if (pressureTarget - state.smoothedPressure > 1.f) outputFlowStarted = true;
        else return;
      }
      if (!predictiveTargetReached && state.smoothedPressure < pressureTarget) {
        return;
      } else {
        predictiveTargetReached = true;
        return;
      }
    }
    // Pressure has to cross the threshold - reduced for lighter roasts
    if (state.smoothedPressure < 1.8f) return;

    if (phaseTypePressure) {
      // If the pressure or flow are raising too fast dismiss the spike from the output.
      if (fabsf(state.pressureChangeSpeed) > 5.f || fabsf(state.pumpFlowChangeSpeed) > 2.f) return;
      // If flow is too big for given pressure or the delta is changing too quickly we're not there yet
      if (resistanceDelta > 500.f || puckResistance < 1100.f) return;
    }

    // If flow is too big for given pressure or the delta is changing too quickly we're not there yet
    // if (puckResistance < 1100.f) return;
    if (truePuckResistance < -0.015f) return;

    // We're there!
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

  }
};

#endif
