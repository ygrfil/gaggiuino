/* 09:32 15/03/2023 - change triggering comment */
#ifndef DEFAULT_PROFILES_H
#define DEFAULT_PROFILES_H

#include <Arduino.h>
#include "eeprom_data.h"

namespace {
  typedef struct {
    char     name[24];
    bool     preinfusionState;
    bool     preinfusionFlowState;
    uint16_t preinfusionSec;
    float    preinfusionBar;
    float    preinfusionFlowVol;
    uint16_t preinfusionFlowTime;
    float    preinfusionFlowPressureTarget;
    float    preinfusionPressureFlowTarget;
    float    preinfusionFilled;
    bool     preinfusionPressureAbove;
    float    preinfusionWeightAbove;
    bool     soakState;
    uint16_t soakTimePressure;
    uint16_t soakTimeFlow;
    float    soakKeepPressure;
    float    soakKeepFlow;
    float    soakBelowPressure;
    float    soakAbovePressure;
    float    soakAboveWeight;
    uint16_t preinfusionRamp;
    uint16_t preinfusionRampSlope;
    bool     tpState;
    bool     tpType;
    float    tpProfilingStart;
    float    tpProfilingFinish;
    uint16_t tpProfilingHold;
    float    tpProfilingHoldLimit;
    uint16_t tpProfilingSlope;
    uint16_t tpProfilingSlopeShape;
    float    tpProfilingFlowRestriction;
    float    tfProfileStart;
    float    tfProfileEnd;
    uint16_t tfProfileHold;
    float    tfProfileHoldLimit;
    uint16_t tfProfileSlope;
    uint16_t tfProfileSlopeShape;
    float    tfProfilingPressureRestriction;
    bool     profilingState;
    bool     mfProfileState;
    float    mpProfilingStart;
    float    mpProfilingFinish;
    uint16_t mpProfilingSlope;
    uint16_t mpProfilingSlopeShape;
    float    mpProfilingFlowRestriction;
    float    mfProfileStart;
    float    mfProfileEnd;
    uint16_t mfProfileSlope;
    uint16_t mfProfileSlopeShape;
    float    mfProfilingPressureRestriction;
    /*-----------OTHER---------------------*/
    uint16_t setpoint;
    bool     stopOnWeightState;
    float    shotDose;
    float    shotStopOnCustomWeight;
    uint16_t shotPreset;
  } profileDefaults_t;

  // DONE:: Fully customised default profiles in line wiuth the names
  const profileDefaults_t defaultsProfile[MAX_PROFILES] = {
    {"Rao Best Practice",/*pi*/true, true, 0, 0.f, 3.f, 5, 3.f, 0.f, 600, true, 0.f,/*sk*/true, 8, 8, 0.f, 0.f, 4.f, 0.f, 0.f, 1, 2,/*tp*/true, false, 4.f, 9.f, 0, 0.f, 6, 0, 0.f, 0.f, 2.5f, 0, 9.f, 6, 0, 9.f,/*pf*/true, true, 0.f, 0.f, 20, 0, 2.5f, 2.5f, 2.f, 20, 0, 9.f,/*other*/ 92, false, 18.f, 0.f, 2},  // profile 0 - Scott Rao's best practice
    {"Dark Roast Master",/*pi*/true, false, 4, 2.5f, 0.f, 0, 0.f, 0.f, 600, false, 0.f,/*sk*/false, 0, 0, 0.f, 0.f, 0.f, 0.f, 0.f, 3, 2,/*tp*/true, false, 2.5f, 6.5f, 2, 0.f, 3, 0, 0.f, 0.f, 0.f, 0, 0.f, 0, 0, 0.f,/*pf*/true, false, 6.5f, 6.f, 22, 0, 2.f, 0.f, 0.f, 0, 0, 0.f,/*other*/ 88, false, 19.f, 0.f, 2},  // profile 1 - Dark roast optimized
    {"Thick Syrup",/*pi*/true, true, 0, 0.f, 1.5f, 8, 4.f, 0.f, 600, true, 0.f,/*sk*/true, 0, 6, 0.f, 1.0f, 0.f, 0.f, 0.f, 1, 2,/*tp*/false, false, 0.f, 0.f, 0, 0.f, 0, 0, 0.f, 0.f, 0.f, 0, 0.f, 0, 0, 0.f,/*pf*/true, true, 0.f, 0.f, 0, 0, 0.f, 1.0f, 1.5f, 30, 0, 10.f,/*other*/ 94, false, 18.f, 0.f, 2},  // profile 2 - Ultra slow flow for thick body
    {"Thick Ristretto",/*pi*/true, false, 4, 3.5f, 0.f, 0, 0.f, 0.f, 600, false, 0.f,/*sk*/false, 0, 0, 0.f, 0.f, 0.f, 0.f, 0.f, 2, 2,/*tp*/true, false, 3.5f, 10.f, 0, 0.f, 3, 0, 1.5f, 0.f, 0.f, 0, 0.f, 0, 0, 0.f,/*pf*/true, false, 10.f, 8.5f, 18, 0, 1.5f, 0.f, 0.f, 0, 0, 0.f,/*other*/ 92, false, 18.f, 0.f, 2},  // profile 3 - High pressure concentrated shot
    {"Thick Bloom",/*pi*/true, true, 0, 0.f, 1.0f, 10, 4.f, 0.f, 600, true, 0.f,/*sk*/true, 0, 12, 0.f, 0.8f, 0.f, 0.f, 0.f, 1, 2,/*tp*/false, false, 0.f, 0.f, 0, 0.f, 0, 0, 0.f, 0.f, 0.f, 0, 0.f, 0, 0, 0.f,/*pf*/true, true, 0.f, 0.f, 0, 0, 0.f, 0.8f, 1.2f, 35, 0, 10.f,/*other*/ 93, false, 18.f, 0.f, 2}  // profile 4 - Long bloom for maximum extraction
  };
}

#endif
