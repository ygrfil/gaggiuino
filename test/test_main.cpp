#include <unity.h>
#include "./tests/test_pressure_profiler.cpp"
#include "./tests/test_pump.cpp"
#include "./tests/test_profile_serializer.cpp"
#include "./utils/test-utils.cpp"

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    runAllPressureProfilerTests();
    runAllPumpTests();
    runAllProfileSerializerTests();
    UNITY_END();
}
