#include "eeprom_data.h"
#include <FlashStorage_STM32.h>
#include "eeprom_metadata.h"
#include "default_profiles.h"
#include "legacy/eeprom_data_v4.h"
#include "legacy/eeprom_data_v5.h"
#include "legacy/eeprom_data_v6.h"
#include "legacy/eeprom_data_v7.h"
#include "legacy/eeprom_data_v8.h"
#include "legacy/eeprom_data_v9.h"
#include "legacy/eeprom_data_v10.h"
#include "legacy/eeprom_data_v11.h"
#include "../log.h"

namespace {

  struct eepromMetadata_t eepromMetadata;

  eepromValues_t getEepromDefaults(void) {
    eepromValues_t defaultData;

    // Profiles - copy fields from default profile template
    defaultData.activeProfile = 0;
    for (int i = 0; i < MAX_PROFILES; i++) {
      auto& dst = defaultData.profiles[i];
      const auto& src = defaultsProfile[i];
      snprintf(dst.name, PROFILE_NAME_LENGTH, "%s", src.name);
      dst.preinfusionState = src.preinfusionState;
      dst.preinfusionFlowState = src.preinfusionFlowState;
      dst.preinfusionSec = src.preinfusionSec;
      dst.preinfusionBar = src.preinfusionBar;
      dst.preinfusionFlowVol = src.preinfusionFlowVol;
      dst.preinfusionFlowTime = src.preinfusionFlowTime;
      dst.preinfusionFlowPressureTarget = src.preinfusionFlowPressureTarget;
      dst.preinfusionPressureFlowTarget = src.preinfusionPressureFlowTarget;
      dst.preinfusionFilled = src.preinfusionFilled;
      dst.preinfusionPressureAbove = src.preinfusionPressureAbove;
      dst.preinfusionWeightAbove = src.preinfusionWeightAbove;
      dst.soakState = src.soakState;
      dst.soakTimePressure = src.soakTimePressure;
      dst.soakTimeFlow = src.soakTimeFlow;
      dst.soakKeepPressure = src.soakKeepPressure;
      dst.soakKeepFlow = src.soakKeepFlow;
      dst.soakBelowPressure = src.soakBelowPressure;
      dst.soakAbovePressure = src.soakAbovePressure;
      dst.soakAboveWeight = src.soakAboveWeight;
      dst.preinfusionRamp = src.preinfusionRamp;
      dst.preinfusionRampSlope = src.preinfusionRampSlope;
      dst.tpState = src.tpState;
      dst.tpType = src.tpType;
      dst.tpProfilingStart = src.tpProfilingStart;
      dst.tpProfilingFinish = src.tpProfilingFinish;
      dst.tpProfilingHold = src.tpProfilingHold;
      dst.tpProfilingHoldLimit = src.tpProfilingHoldLimit;
      dst.tpProfilingSlope = src.tpProfilingSlope;
      dst.tpProfilingSlopeShape = src.tpProfilingSlopeShape;
      dst.tpProfilingFlowRestriction = src.tpProfilingFlowRestriction;
      dst.tfProfileStart = src.tfProfileStart;
      dst.tfProfileEnd = src.tfProfileEnd;
      dst.tfProfileHold = src.tfProfileHold;
      dst.tfProfileHoldLimit = src.tfProfileHoldLimit;
      dst.tfProfileSlope = src.tfProfileSlope;
      dst.tfProfileSlopeShape = src.tfProfileSlopeShape;
      dst.tfProfilingPressureRestriction = src.tfProfilingPressureRestriction;
      dst.profilingState = src.profilingState;
      dst.mfProfileState = src.mfProfileState;
      dst.mpProfilingStart = src.mpProfilingStart;
      dst.mpProfilingFinish = src.mpProfilingFinish;
      dst.mpProfilingSlope = src.mpProfilingSlope;
      dst.mpProfilingSlopeShape = src.mpProfilingSlopeShape;
      dst.mpProfilingFlowRestriction = src.mpProfilingFlowRestriction;
      dst.mfProfileStart = src.mfProfileStart;
      dst.mfProfileEnd = src.mfProfileEnd;
      dst.mfProfileSlope = src.mfProfileSlope;
      dst.mfProfileSlopeShape = src.mfProfileSlopeShape;
      dst.mfProfilingPressureRestriction = src.mfProfilingPressureRestriction;
      dst.setpoint = src.setpoint;
      dst.stopOnWeightState = src.stopOnWeightState;
      dst.shotDose = src.shotDose;
      dst.shotStopOnCustomWeight = src.shotStopOnCustomWeight;
      dst.shotPreset = src.shotPreset;
    }

    // General brew settings
    defaultData.homeOnShotFinish = false;
    defaultData.brewDeltaState = true;
    defaultData.basketPrefill = false;

    // System settings
    defaultData.steamSetPoint = 155;
    defaultData.offsetTemp = 7;
    defaultData.hpwr = 550;
    defaultData.mainDivider = 5;
    defaultData.brewDivider = 3;
    defaultData.powerLineFrequency = 50;
    defaultData.lcdSleep = 16;
    defaultData.warmupState = false;
    defaultData.scalesF1 = 3920;
    defaultData.scalesF2 = 4210;
    defaultData.pumpFlowAtZero = 0.2225f;
    defaultData.ledState = true;
    defaultData.ledDisco = true;
    defaultData.ledR = 9;
    defaultData.ledG = 0;
    defaultData.ledB = 9;

    return defaultData;
  }

  // kind of annoying, but allows reusing macro without messing up type safety
  template <typename T>
  bool copy_t(T& target, T& source) {
    target = source;
    return true;
  }

  bool loadCurrentEepromData EEPROM_METADATA_LOADER(EEPROM_DATA_VERSION, eepromMetadata_t, copy_t);

}


bool eepromWrite(eepromValues_t eepromValuesNew) {
  const char *errMsg = "Data out of range";
  /* Check various profile array values */
  for (int i=0; i<MAX_PROFILES; i++) {
    if ( eepromValuesNew.profiles[i].preinfusionFlowVol < 0.f
      || eepromValuesNew.profiles[i].mfProfileStart < 0.f
      || eepromValuesNew.profiles[i].mfProfileEnd < 0.f
      || eepromValuesNew.profiles[i].mpProfilingStart < 0.f
      || eepromValuesNew.profiles[i].mpProfilingFinish < 0.f
      || eepromValuesNew.profiles[i].setpoint < 1)
    {
      LOG_ERROR(errMsg);
      return false;
    }
  }
  /* Check various global values */
  if (eepromValuesNew.steamSetPoint < 1
  || eepromValuesNew.steamSetPoint > 165
  || eepromValuesNew.mainDivider < 1
  || eepromValuesNew.brewDivider < 1
  || eepromValuesNew.pumpFlowAtZero < 0.210f
  || eepromValuesNew.pumpFlowAtZero > 0.310f
  || eepromValuesNew.scalesF1 < -20000
  || eepromValuesNew.scalesF2 > 20000)
  {
    LOG_ERROR(errMsg);
    return false;
  }

  /* Saving the values struct + validation and versioning metadata */
  eepromMetadata.timestamp = millis();
  eepromMetadata.version = EEPROM_DATA_VERSION;
  eepromMetadata.values = eepromValuesNew;
  eepromMetadata.versionTimestampXOR = eepromMetadata.timestamp ^ eepromMetadata.version;
  EEPROM.put(0, eepromMetadata);

  return true;
}

void eepromInit(void) {
  // initialiaze defaults on memory
  eepromMetadata.values = getEepromDefaults();

  // read version
  uint16_t version;
  EEPROM.get(0, version);

  // load appropriate version (including current)
  bool readSuccess = false;

  if (version < EEPROM_DATA_VERSION && legacyEepromDataLoaders[version] != nullptr) {
    readSuccess = (*legacyEepromDataLoaders[version])(eepromMetadata.values);
  } else {
    readSuccess = loadCurrentEepromData(eepromMetadata.values);
  }

  if (!readSuccess) {
    LOG_ERROR("SECU_CHECK FAILED! Applying defaults! eepromMetadata.version=%d", version);
    eepromMetadata.values = getEepromDefaults();
  }

  if (!readSuccess || version != EEPROM_DATA_VERSION) {
    eepromWrite(eepromMetadata.values);
  }
}

struct eepromValues_t eepromGetCurrentValues(void) {
  return eepromMetadata.values;
}
struct eepromValues_t eepromGetDefaultValues(void) {
  return getEepromDefaults();
}
