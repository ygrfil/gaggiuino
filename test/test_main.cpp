#include <unity.h>
#include "./tests/test_default_profiles.cpp"
#include "./tests/test_inactivity_tracker.cpp"
#include "./tests/test_pid_controller.cpp"
#include "./tests/test_pressure_controller.cpp"
#include "./tests/test_pressure_profiler.cpp"
#include "./tests/test_pump.cpp"
#include "./tests/test_profile_serializer.cpp"

int main(int argc, char **argv) {
    UNITY_BEGIN();
    runAllDefaultProfileTests();
    runAllInactivityTrackerTests();
    runAllPidControllerTests();
    runAllPressureControllerTests();
    runAllPressureProfilerTests();
    runAllPumpTests();
    runAllProfileSerializerTests();
    UNITY_END();
}
