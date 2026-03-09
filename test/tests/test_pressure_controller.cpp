#include <unity.h>
#include "pressure_controller.h"

void test_pressure_controller_respects_startup_floor_and_max_output(void) {
  PressureController controller;

  const float first = controller.update(
    PressureControllerInput{
      .targetPressure = 9.0f,
      .currentPressure = 0.2f,
      .pressureChangeSpeed = 0.0f,
      .basePumpPct = 0.0f,
      .maxPumpPct = 0.30f
    },
    0
  );

  TEST_ASSERT_TRUE(first >= 0.22f);
  TEST_ASSERT_TRUE(first <= 0.30f);
}

void test_pressure_controller_recovers_when_pressure_overshoots(void) {
  PressureController controller;

  const float startup = controller.update(
    PressureControllerInput{
      .targetPressure = 9.0f,
      .currentPressure = 0.2f,
      .pressureChangeSpeed = 0.0f,
      .basePumpPct = 0.0f,
      .maxPumpPct = 1.0f
    },
    0
  );

  const float settled = controller.update(
    PressureControllerInput{
      .targetPressure = 9.0f,
      .currentPressure = 9.8f,
      .pressureChangeSpeed = 0.3f,
      .basePumpPct = 0.15f,
      .maxPumpPct = 1.0f
    },
    1000
  );

  TEST_ASSERT_TRUE(settled < startup);
  TEST_ASSERT_TRUE(settled >= 0.0f);
}

void test_pressure_controller_turns_off_for_zero_target(void) {
  PressureController controller;
  controller.update(
    PressureControllerInput{
      .targetPressure = 9.0f,
      .currentPressure = 1.0f,
      .pressureChangeSpeed = 0.0f,
      .basePumpPct = 0.0f,
      .maxPumpPct = 1.0f
    },
    0
  );

  TEST_ASSERT_EQUAL_FLOAT(0.0f, controller.update(
    PressureControllerInput{
      .targetPressure = 0.0f,
      .currentPressure = 1.0f,
      .pressureChangeSpeed = 0.0f,
      .basePumpPct = 0.0f,
      .maxPumpPct = 1.0f
    },
    1000
  ));
}

void runAllPressureControllerTests(void) {
  RUN_TEST(test_pressure_controller_respects_startup_floor_and_max_output);
  RUN_TEST(test_pressure_controller_recovers_when_pressure_overshoots);
  RUN_TEST(test_pressure_controller_turns_off_for_zero_target);
}
