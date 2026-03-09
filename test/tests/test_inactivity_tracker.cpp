#include <unity.h>
#include "inactivity_tracker.h"

void test_inactivity_tracker_enters_and_exits_standby(void) {
  InactivityTracker tracker(1500000UL);

  tracker.reset(0);
  tracker.update(1499999UL, false);
  TEST_ASSERT_FALSE(tracker.isStandby());

  tracker.update(1500000UL, false);
  TEST_ASSERT_TRUE(tracker.isStandby());

  tracker.noteActivity(1500001UL);
  TEST_ASSERT_FALSE(tracker.isStandby());
}

void test_inactivity_tracker_busy_machine_resets_idle_window(void) {
  InactivityTracker tracker(1000UL);

  tracker.reset(0);
  tracker.update(900UL, true);
  tracker.update(1500UL, false);
  TEST_ASSERT_FALSE(tracker.isStandby());

  tracker.update(1900UL, false);
  TEST_ASSERT_TRUE(tracker.isStandby());
}

void runAllInactivityTrackerTests(void) {
  RUN_TEST(test_inactivity_tracker_enters_and_exits_standby);
  RUN_TEST(test_inactivity_tracker_busy_machine_resets_idle_window);
}
