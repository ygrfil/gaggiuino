#include <unity.h>
#include "pid_controller.h"

void test_pid_controller_clamps_and_recovers_without_windup(void) {
  PIDController controller(PIDConfig{
    .kp = 2.0f,
    .ki = 1.0f,
    .kd = 0.0f,
    .outputMin = 0.0f,
    .outputMax = 10.0f,
    .integralMin = -2.0f,
    .integralMax = 2.0f,
    .derivativeAlpha = 0.2f
  });

  TEST_ASSERT_EQUAL_FLOAT(10.0f, controller.update(10.0f, 0.0f, 0));
  TEST_ASSERT_EQUAL_FLOAT(10.0f, controller.update(10.0f, 0.0f, 1000));
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, controller.update(10.0f, 10.0f, 2000));
}

void test_pid_controller_reset_clears_previous_state(void) {
  PIDController controller(PIDConfig{
    .kp = 1.0f,
    .ki = 0.5f,
    .kd = 0.0f,
    .outputMin = 0.0f,
    .outputMax = 100.0f,
    .integralMin = -10.0f,
    .integralMax = 10.0f,
    .derivativeAlpha = 0.2f
  });

  controller.update(100.0f, 90.0f, 0);
  controller.update(100.0f, 90.0f, 1000);
  controller.reset(90.0f);

  TEST_ASSERT_EQUAL_FLOAT(10.0f, controller.update(100.0f, 90.0f, 2000));
}

void runAllPidControllerTests(void) {
  RUN_TEST(test_pid_controller_clamps_and_recovers_without_windup);
  RUN_TEST(test_pid_controller_reset_clears_previous_state);
}
