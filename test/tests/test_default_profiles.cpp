#include <cstring>

#include <unity.h>

#include "../../src/eeprom_data/default_profiles.h"

void test_default_profiles_are_blackpill_ordered(void) {
  TEST_ASSERT_EQUAL_STRING("Rao Best Practice", defaultsProfile[0].name);
  TEST_ASSERT_EQUAL_STRING("Classic 9 Bar", defaultsProfile[1].name);
  TEST_ASSERT_EQUAL_STRING("Light Roast Flow", defaultsProfile[2].name);
  TEST_ASSERT_EQUAL_STRING("Blooming Espresso", defaultsProfile[3].name);
  TEST_ASSERT_EQUAL_STRING("Dark Roast Comfort", defaultsProfile[4].name);
}

void test_default_profiles_use_estimated_shot_stop_targets(void) {
  TEST_ASSERT_TRUE(defaultsProfile[0].stopOnWeightState);
  TEST_ASSERT_TRUE(defaultsProfile[1].stopOnWeightState);
  TEST_ASSERT_TRUE(defaultsProfile[2].stopOnWeightState);
}

void runAllDefaultProfileTests(void) {
  RUN_TEST(test_default_profiles_are_blackpill_ordered);
  RUN_TEST(test_default_profiles_use_estimated_shot_stop_targets);
}
