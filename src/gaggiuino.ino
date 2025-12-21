/* 09:32 15/03/2023 - change triggering comment */
#pragma GCC optimize ("Ofast")
#if defined(DEBUG_ENABLED)
  #include "dbg.h"
#endif
#include "gaggiuino.h"
#include "peripherals/temperature_safety.h"

// Enhanced Kalman filters for smoother pressure profiling and better user experience
SimpleKalmanFilter smoothPressure(0.3f, 0.3f, 0.06f);      // Optimized: faster response with good noise rejection
SimpleKalmanFilter smoothPumpFlow(0.1f, 0.1f, 0.01f);      // Optimized: better flow stability
SimpleKalmanFilter smoothScalesFlow(0.25f, 0.25f, 0.008f); // Optimized: improved weight tracking
SimpleKalmanFilter smoothConsideredFlow(0.1f, 0.1f, 0.04f); // Optimized: smoother flow transitions

//default phases. Updated in updateProfilerPhases.
Profile profile;
PhaseProfiler phaseProfiler{profile};

PredictiveWeight predictiveWeight;

SensorState currentState;

OPERATION_MODES selectedOperationalMode;

eepromValues_t runningCfg;

SystemState systemState;

LED led;
TOF tof;

void setup(void) {
  LOG_INIT();
  LOG_INFO("Gaggiuino (fw: %s) booting", AUTO_VERSION);

  // Various pins operation mode handling
  pinInit();
  LOG_INFO("Pin init");

  setBoilerOff();  // relayPin LOW
  setSteamValveRelayOff();
  setSteamBoilerRelayOff();
  LOG_INFO("Boiler turned off");

  //Pump
  setPumpOff();
  LOG_INFO("Pump turned off");

  // Valve
  closeValve();
  LOG_INFO("Valve closed");

  lcdInit();
  LOG_INFO("LCD Init");

#if defined(DEBUG_ENABLED)
  // Debug init if enabled
  dbgInit();
  LOG_INFO("DBG init");
#endif

  // Initialise comms library for talking to the ESP mcu
  espCommsInit();

  // Initialize LED
  led.begin();
  led.setColor(9u, 0u, 9u); // WHITE
  // Init the tof sensor
  tof.init(currentState);

  // Initialising the saved values or writing defaults if first start
  eepromInit();
  runningCfg = eepromGetCurrentValues();
  LOG_INFO("EEPROM Init");

  cpsInit(runningCfg);
  LOG_INFO("CPS Init");

  thermocoupleInit();
  LOG_INFO("Thermocouple Init");

  lcdUploadCfg(runningCfg);
  LOG_INFO("LCD cfg uploaded");

  adsInit();
  LOG_INFO("Pressure sensor init");

  // Scales handling
  scalesInit(runningCfg.scalesF1, runningCfg.scalesF2);
  LOG_INFO("Scales init");

  // Pump init
  pumpInit(runningCfg.powerLineFrequency, runningCfg.pumpFlowAtZero);
  LOG_INFO("Pump init");

  pageValuesRefresh();
  LOG_INFO("Setup sequence finished");

  // Change LED colour on setup exit.
  led.setColor(9u, 0u, 9u); // 64171

  // Initialize auto-shutdown feature
  lastActivityTime = millis();
  systemState.autoShutdownEnabled = true;  // Enable by default
  systemState.shutdownWarningShown = false;
  systemState.shutdownActive = false;

  iwdcInit();
}

//##############################################################################################################################
//############################################________________MAIN______________################################################
//##############################################################################################################################


//Main loop where all the logic is continuously run
void loop(void) {
  fillBoiler();
  if (lcdCurrentPageId != lcdLastCurrentPageId) pageValuesRefresh();
  lcdListen();
  sensorsRead();
  brewDetect();
  modeSelect();
  lcdRefresh();
  espCommsSendSensorData(currentState);
  sysHealthCheck(SYS_PRESSURE_IDLE);
}

//##############################################################################################################################
//#############################################___________SENSORS_READ________##################################################
//##############################################################################################################################


static void sensorsRead(void) {
  sensorReadSwitches();
  espCommsReadData();
  sensorsReadTemperature();
  sensorsReadWeight();
  sensorsReadPressure();
  calculateWeightAndFlow();
  updateStartupTimer();
  readTankWaterLevel();
  doLed();
}

static void sensorReadSwitches(void) {
  bool previousBrewState = currentState.brewSwitchState;
  bool previousSteamState = currentState.steamSwitchState;
  bool previousHotWaterState = currentState.hotWaterSwitchState;
  
  currentState.brewSwitchState = brewState();
  currentState.steamSwitchState = steamState();
  currentState.hotWaterSwitchState = waterPinState() || (currentState.brewSwitchState && currentState.steamSwitchState); // use either an actual switch, or the GC/GCP switch combo
  
  // Reset activity timer if any switch state changed (user interaction detected)
  if (currentState.brewSwitchState != previousBrewState ||
      currentState.steamSwitchState != previousSteamState ||
      currentState.hotWaterSwitchState != previousHotWaterState) {
    lastActivityTime = millis();
    systemState.shutdownWarningShown = false;  // Reset warning flag on activity
  }
}

static void sensorsReadTemperature(void) {
  if (millis() > thermoTimer) {
    currentState.temperature = thermocoupleRead() - runningCfg.offsetTemp;
    thermoTimer = millis() + GET_KTYPE_READ_EVERY;
  }
}

static void sensorsReadWeight(void) {
  uint32_t elapsedTime = millis() - scalesTimer;

  if (elapsedTime > GET_SCALES_READ_EVERY) {
    currentState.scalesPresent = scalesIsPresent();
    if (currentState.scalesPresent) {
      if (currentState.tarePending) {
        scalesTare();
        weightMeasurements.clear();
        weightMeasurements.add(scalesGetWeight());
        currentState.tarePending = false;
      }
      else {
        weightMeasurements.add(scalesGetWeight());
      }
      currentState.weight = weightMeasurements.latest().value;

      if (brewActive) {
        // Safety: prevent negative weight spikes from affecting shot weight
        // Only update shotWeight from scales when scales are actually present
        if (currentState.scalesPresent) {
          if (!currentState.tarePending && currentState.weight > -0.2f) {
            currentState.shotWeight = fmax(0.f, currentState.weight);
          } else if (currentState.tarePending) {
            currentState.shotWeight = 0.f;
          }
          currentState.weightFlow = fmax(0.f, weightMeasurements.measurementChange().changeSpeed());
          currentState.smoothedWeightFlow = smoothScalesFlow.updateEstimate(currentState.weightFlow);
        }
        // When scales are not present, shotWeight is updated from flow calculations in calculateWeightAndFlow()
        // Do not modify shotWeight here to avoid conflicts
      }
    }
    scalesTimer = millis();
  }
}

static void sensorsReadPressure(void) {
  uint32_t elapsedTime = millis() - pressureTimer;

  if (elapsedTime > GET_PRESSURE_READ_EVERY) {
    float elapsedTimeSec = elapsedTime / 1000.f;
    currentState.pressure = getPressure();
    previousSmoothedPressure = currentState.smoothedPressure;
    currentState.smoothedPressure = smoothPressure.updateEstimate(currentState.pressure);
    currentState.pressureChangeSpeed = (currentState.smoothedPressure - previousSmoothedPressure) / elapsedTimeSec;
    pressureTimer = millis();
  }
}

static long sensorsReadFlow(float elapsedTimeSec) {
  long pumpClicks = getAndResetClickCounter();
  currentState.pumpClicks = (float) pumpClicks / elapsedTimeSec;

  currentState.pumpFlow = getPumpFlow(currentState.pumpClicks, currentState.smoothedPressure);

  previousSmoothedPumpFlow = currentState.smoothedPumpFlow;
  // Some flow smoothing
  currentState.smoothedPumpFlow = smoothPumpFlow.updateEstimate(currentState.pumpFlow);
  currentState.pumpFlowChangeSpeed = (currentState.smoothedPumpFlow - previousSmoothedPumpFlow) / elapsedTimeSec;
  return pumpClicks;
}

static void calculateWeightAndFlow(void) {
  uint32_t elapsedTime = millis() - flowTimer;

  if (brewActive) {
    // Marking for tare in case smth has gone wrong and it has exited tare already.
    if (currentState.weight < -.3f) currentState.tarePending = true;

    if (elapsedTime > REFRESH_FLOW_EVERY) {
      flowTimer = millis();
      float elapsedTimeSec = elapsedTime / 1000.f;
      long pumpClicks = sensorsReadFlow(elapsedTimeSec);
      float consideredFlow = currentState.smoothedPumpFlow * elapsedTimeSec;
      
      // Always track water pumped during brewing
      currentState.waterPumped += consideredFlow;
      
      // Calculate flow for display and weight prediction
      float flowPerClick = getPumpFlowPerClick(currentState.smoothedPressure);
      float actualFlow = (consideredFlow > pumpClicks * flowPerClick) ? consideredFlow : pumpClicks * flowPerClick;
      
      // Apply flow reduction for ramp-up phase if needed
      if ((ACTIVE_PROFILE(runningCfg).mfProfileState || ACTIVE_PROFILE(runningCfg).tpType) && currentState.pressureChangeSpeed > 0.15f) {
        if ((currentState.smoothedPressure < ACTIVE_PROFILE(runningCfg).mfProfileStart * 0.9f)
        || (currentState.smoothedPressure < ACTIVE_PROFILE(runningCfg).tfProfileStart * 0.9f)) {
          actualFlow *= 0.3f;
        }
      }
      
      currentState.consideredFlow = smoothConsideredFlow.updateEstimate(actualFlow);
      
      // SIMPLIFIED WEIGHT PREDICTION: For non-scale setups, accumulate weight from flow immediately
      // No complex algorithm delays - just accumulate flow directly when brew is active
      // This ensures weight prediction works reliably without interfering with brewing
      // Removed all predictive algorithm dependencies - simple accumulation only
      if (!currentState.scalesPresent && actualFlow > 0.f) {
        currentState.shotWeight = currentState.shotWeight + actualFlow;
      }
      // When scales are present, shotWeight is updated from actual weight readings in sensorsReadWeight()
    }
  } else {
    currentState.consideredFlow = 0.f;
    currentState.pumpClicks = getAndResetClickCounter();
    flowTimer = millis();
  }
}

// return the reading in mm of the tank water level.
static void readTankWaterLevel(void) {
  if (lcdCurrentPageId == NextionPage::Home) {
    // static uint32_t tof_timeout = millis();
    // if (millis() >= tof_timeout) {
    currentState.waterLvl = tof.readLvl();
      // tof_timeout = millis() + 500;
    // }
  }
}

//##############################################################################################################################
//############################################______PAGE_CHANGE_VALUES_REFRESH_____#############################################
//##############################################################################################################################
static void pageValuesRefresh() {
  // Track LCD navigation as user activity for auto-shutdown
  if (lcdCurrentPageId != lcdLastCurrentPageId) {
    lastActivityTime = millis();
    systemState.shutdownWarningShown = false;
  }
  
  // Read the page we're landing in: leaving keyboard page means a value could've changed in it
  if (lcdLastCurrentPageId == NextionPage::KeyboardNumeric) lcdFetchPage(runningCfg, lcdCurrentPageId, runningCfg.activeProfile);
  // Or maybe it's a page that needs constant polling
  else if (lcdLastCurrentPageId == NextionPage::Led) lcdFetchPage(runningCfg, lcdCurrentPageId, runningCfg.activeProfile);
  // Finally read the page we left, as it could've been changed in place (e.g. boolean toggles)
  else lcdFetchPage(runningCfg, lcdLastCurrentPageId, runningCfg.activeProfile);

  homeScreenScalesEnabled = lcdGetHomeScreenScalesEnabled();
  // MODE_SELECT should always be LAST
  selectedOperationalMode = (OPERATION_MODES) lcdGetSelectedOperationalMode();

  // CRITICAL FIX: Never rebuild profile phases while brew is active!
  // This prevents race condition where profile gets cleared mid-brew causing immediate termination
  // If brew is active, profile phases are already set and should not be modified
  if (!brewActive) {
    updateProfilerPhases();
  }

  lcdLastCurrentPageId = lcdCurrentPageId;
}

//#############################################################################################
//############################____OPERATIONAL_MODE_CONTROL____#################################
//#############################################################################################
static void modeSelect(void) {
  if (!systemState.startupInitFinished) {
    // Maintain brew temperature even during startup/boiler fill
    justDoCoffee(runningCfg, currentState, false);
    return;
  }

  switch (selectedOperationalMode) {
    //REPLACE ALL THE BELOW WITH OPMODE_auto_profiling
    case OPERATION_MODES::OPMODE_straight9Bar:
    case OPERATION_MODES::OPMODE_justPreinfusion:
    case OPERATION_MODES::OPMODE_justPressureProfile:
    case OPERATION_MODES::OPMODE_preinfusionAndPressureProfile:
    case OPERATION_MODES::OPMODE_flowPreinfusionStraight9BarProfiling:
    case OPERATION_MODES::OPMODE_justFlowBasedProfiling:
    case OPERATION_MODES::OPMODE_FlowBasedPreinfusionPressureBasedProfiling:
    case OPERATION_MODES::OPMODE_everythingFlowProfiled:
    case OPERATION_MODES::OPMODE_pressureBasedPreinfusionAndFlowProfile:
      nonBrewModeActive = false;
      if (currentState.hotWaterSwitchState) hotWaterMode(currentState);
      else if (currentState.steamSwitchState) steamCtrl(runningCfg, currentState);
      else {
        profiling();
        steamTime = millis();
      }
      break;
    case OPERATION_MODES::OPMODE_manual:
      nonBrewModeActive = false;
      if (!currentState.steamSwitchState) steamTime = millis();
      manualFlowControl();
      break;
    case OPERATION_MODES::OPMODE_flush:
      nonBrewModeActive = true;
      if (!currentState.steamSwitchState) steamTime = millis();
      backFlush(currentState);
      brewActive ? setBoilerOff() : justDoCoffee(runningCfg, currentState, false);
      break;
    case OPERATION_MODES::OPMODE_steam:
      nonBrewModeActive = true;
      steamCtrl(runningCfg, currentState);

      if (!currentState.steamSwitchState) {
        brewActive ? flushActivated() : flushDeactivated();
        steamCtrl(runningCfg, currentState);
        pageValuesRefresh();
      }
      break;
    case OPERATION_MODES::OPMODE_descale:
      nonBrewModeActive = true;
      if (!currentState.steamSwitchState) steamTime = millis();
      deScale(runningCfg, currentState);
      break;
    default:
      pageValuesRefresh();
      break;
  }
}

//#############################################################################################
//################################____LCD_REFRESH_CONTROL___###################################
//#############################################################################################

static void lcdRefresh(void) {
  uint16_t tempDecimal;

  if (millis() > pageRefreshTimer) {
    /*LCD pressure output, as a measure to beautify the graphs locking the live pressure read for the LCD alone*/
    #ifdef BEAUTIFY_GRAPH
      lcdSetPressure(currentState.smoothedPressure * 10.f);
    #else
      lcdSetPressure(
        currentState.pressure > 0.f
          ? currentState.pressure * 10.f
          : 0.f
      );
    #endif

    /*LCD temp output*/
    float brewTempSetPoint = ACTIVE_PROFILE(runningCfg).setpoint + runningCfg.offsetTemp;
    // float liveTempWithOffset = currentState.temperature - runningCfg.offsetTemp;
    // CRITICAL FIX: Prevent division by zero in water temperature calculation
    if (currentState.temperature > (float)ACTIVE_PROFILE(runningCfg).setpoint && currentState.brewSwitchState && brewTempSetPoint > 0.1f) {
      currentState.waterTemperature = currentState.temperature / (float)brewTempSetPoint + (float)ACTIVE_PROFILE(runningCfg).setpoint;
    } else {
      currentState.waterTemperature = currentState.temperature;
    }

    lcdSetTemperature(std::floor((uint16_t)currentState.waterTemperature));

    /*LCD weight & temp & water lvl output*/
    switch (lcdCurrentPageId) {
      case NextionPage::Home:
        // temp decimal handling
        tempDecimal = (currentState.waterTemperature - (uint16_t)currentState.waterTemperature) * 10;
        lcdSetTemperatureDecimal(tempDecimal);
        // water lvl
        lcdSetTankWaterLvl(currentState.waterLvl);
        //weight
        if (homeScreenScalesEnabled) lcdSetWeight(currentState.weight);
        break;
      case NextionPage::BrewGraph:
      case NextionPage::BrewManual:
        // temp decimal handling
        tempDecimal = (currentState.waterTemperature - (uint16_t)currentState.waterTemperature) * 10;
        lcdSetTemperatureDecimal(tempDecimal);
        // SIMPLIFIED: Always show shotWeight when brewing, no complex conditions
        // Display weight (handle negative values for tare indication)
        if (currentState.shotWeight > -0.8f) {
          lcdSetWeight(currentState.shotWeight);
        } else {
          lcdSetWeight(-0.9f); // Show tare needed
        }
        /*LCD flow output*/
        lcdSetFlow( currentState.smoothedPumpFlow * 10.f);
        break;
      default:
        break; // don't push needless data on other pages
    }

  #ifdef DEBUG_ENABLED
    lcdShowDebug(readTempSensor(), getAdsError());
  #endif

    /*LCD timer and warmup*/
    if (brewActive) {
      lcdSetBrewTimer((millis() > brewingTimer) ? (int)((millis() - brewingTimer) / 1000) : 0);
      lcdBrewTimerStart(); // nextion timer start
      lcdWarmupStateStop(); // Flagging warmup notification on Nextion needs to stop (if enabled)
    } else {
      lcdBrewTimerStop(); // nextion timer stop
    }

    pageRefreshTimer = millis() + REFRESH_SCREEN_EVERY;
  }
}
//#############################################################################################
//###################################____SAVE_BUTTON____#######################################
//#############################################################################################
void tryEepromWrite(const eepromValues_t &eepromValues) {
  bool success = eepromWrite(eepromValues);
  watchdogReload(); // reload the watchdog timer on expensive operations
  if (success) {
    lcdShowPopup("Update successful!");
  } else {
    lcdShowPopup("Data out of range!");
  }
}

void lcdSwitchActiveToStoredProfile(const eepromValues_t & storedSettings) {
  runningCfg.activeProfile = lcdGetSelectedProfile();
  ACTIVE_PROFILE(runningCfg) = storedSettings.profiles[runningCfg.activeProfile];
  // CRITICAL FIX: Never rebuild profile phases during active brewing
  if (!brewActive) {
    updateProfilerPhases();
    // CRITICAL FIX: Ensure profile is fully initialized after switching
    // Reset phase profiler state to ensure clean start for next brew
    phaseProfiler.reset();
    // CRITICAL FIX: Validate profile has phases - if empty, rebuild with defaults
    // This prevents issues with corrupted or invalid profile configurations
    if (profile.phaseCount() == 0) {
      LOG_ERROR("Profile %d has no phases after switch, rebuilding with defaults", runningCfg.activeProfile);
      updateProfilerPhases();
    }
  }
  lcdUploadProfile(runningCfg);
}

// Save the desired temp values to EEPROM
void lcdSaveSettingsTrigger(void) {
  LOG_VERBOSE("Saving values to EEPROM");

  eepromValues_t eepromCurrentValues = eepromGetCurrentValues();
  lcdFetchPage(eepromCurrentValues, lcdCurrentPageId, runningCfg.activeProfile);
  tryEepromWrite(eepromCurrentValues);
}

void lcdSaveProfileTrigger(void) {
  LOG_VERBOSE("Saving profile to EEPROM");

  eepromValues_t eepromCurrentValues = eepromGetCurrentValues();
  lcdFetchCurrentProfile(eepromCurrentValues);
  tryEepromWrite(eepromCurrentValues);
}

void lcdResetSettingsTrigger(void) {
  tryEepromWrite(eepromGetDefaultValues());
}

void lcdLoadDefaultProfileTrigger(void) {
  lcdSwitchActiveToStoredProfile(eepromGetDefaultValues());

  lcdShowPopup("Profile loaded!");
}

void lcdScalesTareTrigger(void) {
  LOG_VERBOSE("Tare scales");
  if (currentState.scalesPresent) currentState.tarePending = true;
}

void lcdHomeScreenScalesTrigger(void) {
  LOG_VERBOSE("Scales enabled or disabled");
  homeScreenScalesEnabled = lcdGetHomeScreenScalesEnabled();
}

void lcdBrewGraphScalesTareTrigger(void) {
  LOG_VERBOSE("Predictive scales tare action completed!");
  if (currentState.scalesPresent) {
    currentState.tarePending = true;
  }
  else {
    // SIMPLIFIED: Just reset weight, no complex predictive algorithm manipulation
    currentState.shotWeight = 0.f;
  }
}

void lcdRefreshElementsTrigger(void) {

  eepromValues_t eepromCurrentValues = eepromGetCurrentValues();

  switch (lcdCurrentPageId) {
    case NextionPage::BrewPreinfusion:
      ACTIVE_PROFILE(eepromCurrentValues).preinfusionFlowState = lcdGetPreinfusionFlowState();
      break;
    case NextionPage::BrewProfiling:
      ACTIVE_PROFILE(eepromCurrentValues).mfProfileState = lcdGetProfileFlowState();
      break;
    case NextionPage::BrewTransitionProfile:
      ACTIVE_PROFILE(eepromCurrentValues).tpType = lcdGetTransitionFlowState();
      break;
    default:
      lcdShowPopup("Nope!");
      break;
  }

  // Make the necessary changes
  uploadPageCfg(eepromCurrentValues, systemState);
  // refresh the screen elements
  pageValuesRefresh();
}

void lcdQuickProfileSwitch(void) {
  // CRITICAL FIX: Prevent profile switching during active brewing
  if (brewActive) {
    return; // Silently ignore profile switch requests during brew
  }
  lcdSwitchActiveToStoredProfile(eepromGetCurrentValues());
  lcdShowPopup("Profile switched!");
}

//#############################################################################################
//###############################____PROFILING_CONTROL____#####################################
//#############################################################################################
static void updateProfilerPhases(void) {
  float shotTarget = -1.f;

  // CRITICAL FIX: Only enable weight-based stop when scales are actually present
  // Without scales, weight calculations are unreliable and can cause premature brew termination
  if (ACTIVE_PROFILE(runningCfg).stopOnWeightState && currentState.scalesPresent) {
    shotTarget = (ACTIVE_PROFILE(runningCfg).shotStopOnCustomWeight < 1.f)
      ? ACTIVE_PROFILE(runningCfg).shotDose * ACTIVE_PROFILE(runningCfg).shotPreset
      : ACTIVE_PROFILE(runningCfg).shotStopOnCustomWeight;
  }

  //update global stop conditions (currently only stopOnWeight is configured in nextion)
  profile.globalStopConditions = GlobalStopConditions{ .weight=shotTarget };

  profile.clear();

  //Setup release pressure + fill@7ml/sec
  if (runningCfg.basketPrefill) {
    addFillBasketPhase(7.f);
  }

  // Setup pre-infusion if needed
  if (ACTIVE_PROFILE(runningCfg).preinfusionState) {
    addPreinfusionPhases();
  }

  // Setup the soak phase if neecessary
  if (ACTIVE_PROFILE(runningCfg).soakState) {
    addSoakPhase();
  }
  preInfusionFinishedPhaseIdx = profile.phaseCount();

  addMainExtractionPhasesAndRamp();
  
  // CRITICAL: After updating profile phases, reinitialize the profiler
  // This ensures currentPhase points to valid phase data
  phaseProfiler.reset();
}

void addPreinfusionPhases() {
  if (ACTIVE_PROFILE(runningCfg).preinfusionFlowState) { // flow based PI enabled
    float isPressureAbove = ACTIVE_PROFILE(runningCfg).preinfusionPressureAbove ? ACTIVE_PROFILE(runningCfg).preinfusionFlowPressureTarget : -1.f;
    float isWeightAbove = ACTIVE_PROFILE(runningCfg).preinfusionWeightAbove > 0.f ? ACTIVE_PROFILE(runningCfg).preinfusionWeightAbove : -1.f;
    float isWaterPumped = ACTIVE_PROFILE(runningCfg).preinfusionFilled > 0.f ? ACTIVE_PROFILE(runningCfg).preinfusionFilled : -1.f;

    addFlowPhase(Transition{ ACTIVE_PROFILE(runningCfg).preinfusionFlowVol }, ACTIVE_PROFILE(runningCfg).preinfusionFlowPressureTarget, ACTIVE_PROFILE(runningCfg).preinfusionFlowTime * 1000, isPressureAbove, -1, isWeightAbove, isWaterPumped);
  }
  else { // pressure based PI enabled
    // For now handling phase switching on restrictions here but as this grow will have to deal with it otherwise.
    float isPressureAbove = ACTIVE_PROFILE(runningCfg).preinfusionPressureAbove ? ACTIVE_PROFILE(runningCfg).preinfusionBar : -1.f;
    float isWeightAbove = ACTIVE_PROFILE(runningCfg).preinfusionWeightAbove > 0.f ? ACTIVE_PROFILE(runningCfg).preinfusionWeightAbove : -1.f;
    float isWaterPumped = ACTIVE_PROFILE(runningCfg).preinfusionFilled > 0.f ? ACTIVE_PROFILE(runningCfg).preinfusionFilled : -1.f;

    addPressurePhase(Transition{ ACTIVE_PROFILE(runningCfg).preinfusionBar }, ACTIVE_PROFILE(runningCfg).preinfusionPressureFlowTarget, ACTIVE_PROFILE(runningCfg).preinfusionSec * 1000, isPressureAbove, -1, isWeightAbove, isWaterPumped);
  }
}

void addSoakPhase() {
    uint16_t phaseSoak = ACTIVE_PROFILE(runningCfg).preinfusionFlowState ? ACTIVE_PROFILE(runningCfg).soakTimeFlow : ACTIVE_PROFILE(runningCfg).soakTimePressure;
    float maintainFlow = ACTIVE_PROFILE(runningCfg).soakKeepFlow > 0.f ? ACTIVE_PROFILE(runningCfg).soakKeepFlow : -1.f;
    float maintainPressure = ACTIVE_PROFILE(runningCfg).soakKeepPressure > 0.f ? ACTIVE_PROFILE(runningCfg).soakKeepPressure : -1.f;
    float isPressureBelow = ACTIVE_PROFILE(runningCfg).soakBelowPressure > 0.f ? ACTIVE_PROFILE(runningCfg).soakBelowPressure : -1.f;
    float isPressureAbove = ACTIVE_PROFILE(runningCfg).soakAbovePressure > 0.f ? ACTIVE_PROFILE(runningCfg).soakAbovePressure : -1.f;
    float isWeightAbove = ACTIVE_PROFILE(runningCfg).soakAboveWeight > 0.f ? ACTIVE_PROFILE(runningCfg).soakAboveWeight : -1.f;

    if (maintainPressure > 0.f)
      addPressurePhase(Transition{maintainPressure}, (maintainFlow > 0.f ? maintainFlow : 2.5f), phaseSoak * 1000, isPressureAbove, isPressureBelow, isWeightAbove, -1);
    else if(maintainFlow > 0.f)
      addFlowPhase(Transition{maintainFlow},  -1, phaseSoak * 1000, isPressureAbove, isPressureBelow, isWeightAbove, -1);
    else
      addPressurePhase(Transition{maintainPressure}, maintainFlow, phaseSoak * 1000, isPressureAbove, isPressureBelow, isWeightAbove, -1);
}

void addMainExtractionPhasesAndRamp() {
  int rampPhaseIndex = -1;

  if (ACTIVE_PROFILE(runningCfg).profilingState) {
    if (ACTIVE_PROFILE(runningCfg).tpState) {
      // ----------------- Transition Profile ----------------- //
      if (ACTIVE_PROFILE(runningCfg).tpType) { // flow based profiling enabled
        /* Setting the phase specific restrictions */
        /* ------------------------------------------ */
        float fpStart = ACTIVE_PROFILE(runningCfg).tfProfileStart;
        float fpEnd = ACTIVE_PROFILE(runningCfg).tfProfileEnd;
        uint16_t fpHold = ACTIVE_PROFILE(runningCfg).tfProfileHold * 1000;
        float holdLimit = ACTIVE_PROFILE(runningCfg).tfProfileHoldLimit > 0.f ? ACTIVE_PROFILE(runningCfg).tfProfileHoldLimit : -1;
        TransitionCurve curve = (TransitionCurve)ACTIVE_PROFILE(runningCfg).tfProfileSlopeShape;
        uint16_t curveTime = ACTIVE_PROFILE(runningCfg).tfProfileSlope * 1000;
        /* ------------------------------------------ */

        if (fpStart > 0.f && fpHold > 0) {
          addFlowPhase(Transition{ fpStart }, holdLimit, fpHold, -1, -1, -1, -1);
          rampPhaseIndex = rampPhaseIndex >= 0 ? rampPhaseIndex : profile.phaseCount() - 1;
        }
        addFlowPhase(Transition{ fpStart, fpEnd, curve, curveTime }, ACTIVE_PROFILE(runningCfg).tfProfilingPressureRestriction, curveTime, -1, -1, -1, -1);
        rampPhaseIndex = rampPhaseIndex >= 0 ? rampPhaseIndex : profile.phaseCount() - 1;
      }
      else { // pressure based profiling enabled
        /* Setting the phase specific restrictions */
        /* ------------------------------------------ */
        float ppStart = ACTIVE_PROFILE(runningCfg).tpProfilingStart;
        float ppEnd = ACTIVE_PROFILE(runningCfg).tpProfilingFinish;
        uint16_t ppHold = ACTIVE_PROFILE(runningCfg).tpProfilingHold * 1000;
        float holdLimit = ACTIVE_PROFILE(runningCfg).tpProfilingHoldLimit > 0.f ? ACTIVE_PROFILE(runningCfg).tpProfilingHoldLimit : -1;
        TransitionCurve curve = (TransitionCurve)ACTIVE_PROFILE(runningCfg).tpProfilingSlopeShape;
        uint16_t curveTime = ACTIVE_PROFILE(runningCfg).tpProfilingSlope * 1000;
        /* ------------------------------------------ */

        if (ppStart > 0.f && ppHold > 0) {
          addPressurePhase(Transition{ ppStart }, holdLimit, ppHold, -1, -1, -1, -1);
          rampPhaseIndex = rampPhaseIndex >= 0 ? rampPhaseIndex : profile.phaseCount() - 1;
        }
        addPressurePhase(Transition{ ppStart, ppEnd, curve, curveTime }, ACTIVE_PROFILE(runningCfg).tpProfilingFlowRestriction, curveTime, -1, -1, -1, -1);
        rampPhaseIndex = rampPhaseIndex >= 0 ? rampPhaseIndex : profile.phaseCount() - 1;
      }
    }

    // ----------------- Main Profile ----------------- //
    if (ACTIVE_PROFILE(runningCfg).mfProfileState) { // flow based profiling enabled
      /* Setting the phase specific restrictions */
      /* ------------------------------------------ */
      float fpStart = ACTIVE_PROFILE(runningCfg).mfProfileStart;
      float fpEnd = ACTIVE_PROFILE(runningCfg).mfProfileEnd;
      TransitionCurve curve = (TransitionCurve)ACTIVE_PROFILE(runningCfg).mfProfileSlopeShape;
      uint16_t curveTime = ACTIVE_PROFILE(runningCfg).mfProfileSlope * 1000;

      /* ------------------------------------------ */
      addFlowPhase(Transition(fpStart, fpEnd, curve, curveTime), ACTIVE_PROFILE(runningCfg).mfProfilingPressureRestriction, -1, -1, -1, -1, -1);
    }
    else { // pressure based profiling enabled
      /* Setting the phase specific restrictions */
      /* ------------------------------------------ */
      float ppStart = ACTIVE_PROFILE(runningCfg).mpProfilingStart;
      float ppEnd = ACTIVE_PROFILE(runningCfg).mpProfilingFinish;
      TransitionCurve curve = (TransitionCurve)ACTIVE_PROFILE(runningCfg).mpProfilingSlopeShape;
      uint16_t curveTime = ACTIVE_PROFILE(runningCfg).mpProfilingSlope * 1000;
      /* ------------------------------------------ */
      addPressurePhase(Transition(ppStart, ppEnd, curve, curveTime), ACTIVE_PROFILE(runningCfg).mpProfilingFlowRestriction, -1, -1, -1, -1, -1);
    }
  } else { // Shot profiling disabled. Default to 9 bars
    addPressurePhase(Transition(9.f), -1, -1, -1, -1, -1, -1);
  }

  // CRITICAL FIX: Prevent array underflow - check if profile has phases before accessing
  if (profile.phaseCount() > 0) {
    rampPhaseIndex = rampPhaseIndex >= 0 ? rampPhaseIndex : profile.phaseCount() - 1;
    insertRampPhaseIfNeeded(rampPhaseIndex);
  }
}

// ------------ Insert a ramp phase in the rampPhaseIndex position ------------ //
void insertRampPhaseIfNeeded(size_t rampPhaseIndex) {
  uint16_t rampTime = ACTIVE_PROFILE(runningCfg).preinfusionRamp;
  TransitionCurve rampCurve = (TransitionCurve)ACTIVE_PROFILE(runningCfg).preinfusionRampSlope;

  // CRITICAL FIX: Check bounds before accessing profile phases
  if (rampPhaseIndex <= 0 || rampPhaseIndex >= profile.phaseCount() || rampTime <= 0 || rampCurve == TransitionCurve::INSTANT) { // No ramp needed
    return;
  }

  // Get the phase currently in rampPhaseIndex - this is the phase we want to ramp to
  Phase targetPhase = profile.phases[rampPhaseIndex];
  float targetValue = targetPhase.target.isInstant() ? targetPhase.target.end : targetPhase.target.start;

  if (targetValue <= 0) { // No ramp needed, next phase will perform a ramp.
    return;
  }

  profile.insertPhase(Phase {
    .type           = targetPhase.type,
    .target         = Transition(targetValue, rampCurve, rampTime * 1000),
    .restriction    = -1,
    .stopConditions = PhaseStopConditions{ .time=rampTime * 1000 }
  }, rampPhaseIndex);
}

void addFillBasketPhase(float flowRate) {
  // CRITICAL FIX: Increased pressure threshold to prevent instant phase completion
  // If residual pressure exists, 0.1 bar would trigger immediately causing brew to abort
  // 2.0 bar is a more realistic threshold for basket fill completion
  // Added 15-second timeout as safety - basket should fill within this time
  addFlowPhase(Transition(flowRate), -1, 15000, 2.0f, -1, -1, -1);
}

void addPressurePhase(Transition pressure, float flowRestriction, int timeMs, float pressureAbove, float pressureBelow, float shotWeight, float isWaterPumped) {
  addPhase(PHASE_TYPE::PHASE_TYPE_PRESSURE, pressure, flowRestriction, timeMs, pressureAbove, pressureBelow, shotWeight, isWaterPumped);
}

void addFlowPhase(Transition flow, float pressureRestriction, int timeMs, float pressureAbove, float pressureBelow, float shotWeight, float isWaterPumped) {
  addPhase(PHASE_TYPE::PHASE_TYPE_FLOW, flow, pressureRestriction, timeMs, pressureAbove, pressureBelow, shotWeight, isWaterPumped);
}

void addPhase(PHASE_TYPE type, Transition target, float restriction, int timeMs, float pressureAbove, float pressureBelow, float shotWeight, float isWaterPumped) {
  profile.addPhase(Phase {
    .type           = type,
    .target         = target,
    .restriction    = restriction,
    .stopConditions = PhaseStopConditions{ .time=timeMs, .pressureAbove=pressureAbove, .pressureBelow=pressureBelow, .weight=shotWeight, .waterPumpedInPhase=isWaterPumped }
  });
}

void onProfileReceived(Profile& newProfile) {
}

static void profiling(void) {
  if (brewActive) { //runs this only when brew button activated and pressure profile selected
    uint32_t timeInShot = (brewingTimer > 0 && millis() >= brewingTimer) 
      ? (millis() - brewingTimer) 
      : 0;
    
    // Safety check - if timeInShot is unreasonable, reset brew state
    if (timeInShot > 7200000) { // More than 2 hours
      LOG_ERROR("Brew timer error: timeInShot=%lu ms", timeInShot);
      brewActive = false;
      setPumpOff();
      closeValve();
      return;
    }
    
    phaseProfiler.updatePhase(timeInShot, currentState);
    CurrentPhase& currentPhase = phaseProfiler.getCurrentPhase();
    ShotSnapshot shotSnapshot = buildShotSnapshot(timeInShot, currentState, currentPhase);
    espCommsSendShotData(shotSnapshot, 100);

    if (phaseProfiler.isFinished()) {
      setPumpOff();
      closeValve();
      brewActive = false;
    } else if (currentPhase.getType() == PHASE_TYPE::PHASE_TYPE_PRESSURE) {
      float newBarValue = currentPhase.getTarget();
      float flowRestriction =  currentPhase.getRestriction();
      openValve();
      setPumpPressure(newBarValue, flowRestriction, currentState);
    } else {
      float newFlowValue = currentPhase.getTarget();
      float pressureRestriction =  currentPhase.getRestriction();
      openValve();
      setPumpFlow(newFlowValue, pressureRestriction, currentState);
    }
  } else {
    setPumpOff();
    closeValve();
  }
  // Keep that water at temp
  justDoCoffee(runningCfg, currentState, brewActive);
}

static void manualFlowControl(void) {
  if (brewActive) {
    openValve();
    float flow_reading = lcdGetManualFlowVol() / 10.f ;
    setPumpFlow(flow_reading, 0.f, currentState);
  } else {
    setPumpOff();
    closeValve();
  }
  justDoCoffee(runningCfg, currentState, brewActive);
}

//#############################################################################################
//###################################____BREW DETECT____#######################################
//#############################################################################################

static void brewDetect(void) {
  // Do not allow brew detection while system reports not ready.
  if (!sysReadinessCheck()) {
    return;
  }

  // Debounced edge-based brew detection for reliable triggering
  static bool lastBrewSwitchState = false;
  static unsigned long lastDebounceTime = 0;
  static bool debouncedState = false;
  const unsigned long DEBOUNCE_DELAY = 30; // 30ms debounce

  bool rawBrewOn = currentState.brewSwitchState;

  // Debounce the switch reading
  if (rawBrewOn != lastBrewSwitchState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // Reading has been stable for debounce period
    bool newDebouncedState = rawBrewOn;

    // Rising edge: start brew immediately and reset parameters once
    if (newDebouncedState && !debouncedState) {
      lcdWakeUp();
      brewParamsReset();
      // Ensure profile is valid before starting brew
      if (profile.phaseCount() == 0) {
        updateProfilerPhases();
      }
      // Only start brewing if we have valid phases
      if (profile.phaseCount() > 0) {
        brewActive = true;
        systemHealthTimer = millis() + HEALTHCHECK_EVERY;
        lcdBrewTimerStart();
      }
    }

    // Falling edge: stop brew and clear counters
    if (!newDebouncedState && debouncedState) {
      brewActive = false;
      currentState.pumpClicks = getAndResetClickCounter();
    }

    debouncedState = newDebouncedState;
  }

  // While brewing, keep system health refreshed to prevent lockups on restrictions
  if (debouncedState) {
    systemHealthTimer = millis() + HEALTHCHECK_EVERY;
  }

  lastBrewSwitchState = rawBrewOn;
}

static void brewParamsReset(void) {
  currentState.tarePending = true;
  currentState.shotWeight  = 0.f;
  currentState.pumpFlow    = 0.f;
  currentState.weight      = 0.f;
  currentState.waterPumped = 0.f;
  brewingTimer             = millis();
  flowTimer                = brewingTimer;
  systemHealthTimer        = brewingTimer + HEALTHCHECK_EVERY;

  weightMeasurements.clear();
  predictiveWeight.reset();
  phaseProfiler.reset();
  
  // Reset temperature safety state when starting a new brew
  temperatureSafety.resetTemperatureStats();
}

static bool sysReadinessCheck(void) {
  // Startup procedures not finished
  if (!systemState.startupInitFinished) {
    return false;
  }
  // If there's not enough water in the tank
  // Fixed: Changed OR to AND - only check water level when NOT on brew pages
  if ((lcdCurrentPageId != NextionPage::BrewGraph && lcdCurrentPageId != NextionPage::BrewManual)
  && currentState.waterLvl < MIN_WATER_LVL)
  {
    lcdShowPopup("Fill the water tank!");
    return false;
  }

  return true;
}

// Auto-shutdown function to save energy and improve safety
static void checkAutoShutdown(void) {
  if (!systemState.autoShutdownEnabled || !systemState.startupInitFinished) {
    return;  // Feature disabled or system not ready
  }

  unsigned long inactivityTime = millis() - lastActivityTime;

  // Enter heater standby after timeout
  if (!systemState.shutdownActive && inactivityTime >= AUTO_SHUTDOWN_TIME) {
    LOG_INFO("Auto heater standby after inactivity");
    lcdShowPopup("Heater standby");
    setBoilerOff();
    setSteamBoilerRelayOff();
    systemState.shutdownActive = true;
    systemState.shutdownWarningShown = false;
  }

  // In standby: keep heater off; any user activity resumes
  if (systemState.shutdownActive) {
    setBoilerOff();
    setSteamBoilerRelayOff();
    if (brewState() || steamState() || waterPinState() || (lcdCurrentPageId != lcdLastCurrentPageId)) {
      systemState.shutdownActive = false;
      lcdShowPopup("");
      lastActivityTime = millis();
      LOG_INFO("Heater resumed from standby");
    }
  }
}

static inline void sysHealthCheck(float pressureThreshold) {
  //Reloading the watchdog timer, if this function fails to run MCU is rebooted
  watchdogReload();
  
  // Check for auto-shutdown
  checkAutoShutdown();
  
  /* This while is here to prevent situations where the system failed to get a temp reading and temp reads as 0 or invalid
  We force the heater OFF while trying to get a temp reading - IMPORTANT safety feature */
  while (currentState.temperature <= 0.0f || isnan(currentState.temperature) || currentState.temperature >= 170.0f) {
    //Reloading the watchdog timer, if this function fails to run MCU is rebooted
    watchdogReload();
    setPumpOff();
    setBoilerOff();
    setSteamBoilerRelayOff();
    if (millis() > thermoTimer) {
      LOG_ERROR("Cannot read temp from thermocouple (last read: %.1lf)!", static_cast<double>(currentState.temperature));
      currentState.steamSwitchState ? lcdShowPopup("COOLDOWN") : lcdShowPopup("TEMP READ ERROR"); // writing a LCD message
      currentState.temperature  = thermocoupleRead() - runningCfg.offsetTemp;  // Making sure we're getting a value
      thermoTimer = millis() + GET_KTYPE_READ_EVERY;
    }
  }

  /* Shut down heaters if steam has been ON and unused for more than 10 minutes. */
  while (currentState.isSteamForgottenON) {
    //Reloading the watchdog timer, if this function fails to run MCU is rebooted
    watchdogReload();
    lcdShowPopup("TURN STEAM OFF NOW!");
    setPumpOff();
    setBoilerOff();
    setSteamBoilerRelayOff();
    currentState.isSteamForgottenON = currentState.steamSwitchState;
  }

  //Releasing the excess pressure after steaming or brewing if necessary
  #if defined LEGO_VALVE_RELAY || defined SINGLE_BOARD

  // No point going through the whole thing if this first condition isn't met.
  // CRITICAL: Also check brewActive to prevent pressure release during active brewing
  if (brewActive || currentState.brewSwitchState || currentState.steamSwitchState || currentState.hotWaterSwitchState) {
    systemHealthTimer = millis() + HEALTHCHECK_EVERY;
    return;
  }
  // Should enter the block every "systemHealthTimer" seconds
  if (millis() >= systemHealthTimer) {
    // Check if pressure release is needed
    if (currentState.smoothedPressure >= pressureThreshold && currentState.temperature < 100.f) {
      // Show popup only once at the start of pressure release
      static bool pressureReleasePopupShown = false;
      if (!pressureReleasePopupShown) {
        lcdShowPopup("Releasing pressure!");
        pressureReleasePopupShown = true;
      }
      
      // Vent pressure using the valve path that drops the measured line pressure
      // (for SINGLE_BOARD this requires opening the controlled valve)
      openValve();
      setPumpOff();
      setBoilerOff();
      setSteamValveRelayOff();
      setSteamBoilerRelayOff();
      
      // Keep checking pressure while releasing - wait until fully vented
      unsigned long pressureReleaseStart = millis();
      float releaseStartPressure = currentState.smoothedPressure;
      unsigned long lastLowPressureTime = 0;
      const float TARGET_LOW_PRESSURE = 0.15f; // Must drop to 0.15 bar or below
      const unsigned long STABLE_LOW_TIME = 500; // Stay low for 500ms to confirm full release
      const unsigned long MAX_RELEASE_TIME = 20000; // Maximum 20 seconds for pressure release
      
      while (currentState.temperature < 100.f)
      {
        //Reloading the watchdog timer, if this function fails to run MCU is rebooted
        watchdogReload();
        
        // CRITICAL: Exit pressure release immediately if user starts brewing
        // This prevents the rare bug where pressing brew during pressure release
        // causes the system to ignore the brew request
        if (brewActive || currentState.brewSwitchState) {
          LOG_INFO("Pressure release aborted - brew started (pressure: %.2f bar)", (double)currentState.smoothedPressure);
          break;
        }
        
        // CRITICAL FIX: Safety timeout to prevent infinite pressure release loops
        // If pressure can't drop below target after 20 seconds, exit anyway
        if (millis() - pressureReleaseStart >= MAX_RELEASE_TIME) {
          LOG_WARN("Pressure release timeout - continuing anyway (pressure: %.2f bar)", (double)currentState.smoothedPressure);
          break;
        }
        
        // Keep reading sensors to update pressure
        sensorsRead();
        
        // Check if pressure is at target low level and stable
        if (currentState.smoothedPressure <= TARGET_LOW_PRESSURE && currentState.pressure <= TARGET_LOW_PRESSURE + 0.1f) {
          // Start or continue timing how long we've been at low pressure
          if (lastLowPressureTime == 0) {
            lastLowPressureTime = millis();
          } else if (millis() - lastLowPressureTime >= STABLE_LOW_TIME) {
            // Pressure has been low and stable - release complete
            LOG_INFO("Pressure fully released (%.2f bar)", (double)currentState.smoothedPressure);
            break;
          }
        } else {
          // Pressure went back up or not low enough yet - reset stability timer
          lastLowPressureTime = 0;
        }

        // Allow brewing pages to continue functioning during pressure release
        switch (lcdCurrentPageId) {
          case NextionPage::BrewManual:
          case NextionPage::BrewGraph:
          case NextionPage::GraphPreview:
            brewDetect();
            lcdRefresh();
            lcdListen();
            justDoCoffee(runningCfg, currentState, brewActive);
            break;
          default:
            // Just keep monitoring pressure on other pages
            break;
        }
        
        // If pressure is trending down significantly, extend timeout window
        if (currentState.smoothedPressure < releaseStartPressure - 0.3f) {
          releaseStartPressure = currentState.smoothedPressure;
          pressureReleaseStart = millis();
        }
      }
      
      // Pressure released - close valve and clear popup
      closeValve();
      pressureReleasePopupShown = false; // Reset for next time
      
      // Clear the popup by showing a brief success message
      lcdShowPopup("Pressure released!");
      delay(500); // Brief delay to show success
      lcdShowPopup(""); // Clear popup
    }
    
    systemHealthTimer = millis() + HEALTHCHECK_EVERY;
  }
  // Throwing a pressure release countodown.
  if (lcdCurrentPageId == NextionPage::BrewGraph) return;
  if (lcdCurrentPageId == NextionPage::BrewManual) return;

  if (currentState.smoothedPressure >= pressureThreshold && currentState.temperature < 100.f) {
    if (millis() >= systemHealthTimer - 3500ul && millis() <= systemHealthTimer - 500ul) {
      char tmp[25];
      int countdown = (int)(systemHealthTimer-millis())/1000;
      unsigned int check = snprintf(tmp, sizeof(tmp), "Dropping beats in: %i", countdown);
      if (check > 0 && check <= sizeof(tmp)) {
        lcdShowPopup(tmp);
      }
    }
  }
  #endif
}

// Function to track time since system has started
static unsigned long getTimeSinceInit(void) {
  static unsigned long startTime = millis();
  return millis() - startTime;
}

static void fillBoiler(void) {
  #if defined LEGO_VALVE_RELAY || defined SINGLE_BOARD

  if (systemState.startupInitFinished) {
    return;
  }

  if (currentState.temperature > BOILER_FILL_SKIP_TEMP) {
    systemState.startupInitFinished = true;
    return;
  }

  if (isBoilerFillPhase(getTimeSinceInit()) && !isSwitchOn()) {
    fillBoilerUntilThreshod(getTimeSinceInit());
  }
  else if (isSwitchOn()) {
    lcdShowPopup("Brew Switch ON!");
  }
#else
  systemState.startupInitFinished = true;
#endif
}

static bool isBoilerFillPhase(unsigned long elapsedTime) {
  return lcdCurrentPageId == NextionPage::Home && elapsedTime >= BOILER_FILL_START_TIME;
}

static bool isBoilerFull(unsigned long elapsedTime) {
  bool boilerFull = false;
  if (elapsedTime > BOILER_FILL_START_TIME + 1000UL) {
    boilerFull =  (previousSmoothedPressure - currentState.smoothedPressure > -0.02f)
                &&
                  (previousSmoothedPressure - currentState.smoothedPressure < 0.001f);
  }

  return elapsedTime >= BOILER_FILL_TIMEOUT || boilerFull;
}

// Checks if Brew switch is ON
static bool isSwitchOn(void) {
  return currentState.brewSwitchState && lcdCurrentPageId == NextionPage::Home;
}

static void fillBoilerUntilThreshod(unsigned long elapsedTime) {
  if (elapsedTime >= BOILER_FILL_TIMEOUT) {
    systemState.startupInitFinished = true;
    return;
  }

  if (isBoilerFull(elapsedTime)) {
    closeValve();
    setPumpOff();
    systemState.startupInitFinished = true;
    return;
  }

  lcdShowPopup("Filling boiler!");
  openValve();
  setPumpToRawValue(35);
}

static void updateStartupTimer(void) {
  lcdSetUpTime(getTimeSinceInit() / 1000);
}

static void cpsInit(eepromValues_t &eepromValues) {
  int cps = getCPS();
  // CPS > 55 indicates 60Hz (single or double), otherwise 50Hz
  if (cps > 0) {
    eepromValues.powerLineFrequency = (cps > 55) ? 60u : 50u;
  }
}

static void doLed(void) {
  if (runningCfg.ledDisco && brewActive) {
    switch(lcdCurrentPageId) {
      case NextionPage::BrewGraph:
      case NextionPage::BrewManual:
        led.setDisco(led.CLASSIC);
        break;
      case NextionPage::Flush:
        led.setDisco(led.STROBE);
        break;
      case NextionPage::Descale:
        led.setDisco(led.DESCALE);
        break;
      default:
        led.setColor(0, 0, 0);
        break;
    }
  } else {
    switch(lcdCurrentPageId) {
      case NextionPage::Led:
        static uint32_t timer = millis();
        if (millis() > timer) {
          timer = millis() + 100u;
          lcdFetchLed(runningCfg);
        }
      default: // intentionally fall through
        led.setColor(runningCfg.ledR, runningCfg.ledG, runningCfg.ledB);
    }
  }
}
