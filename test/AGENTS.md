# Purpose

- Own native Unity tests for shared logic and firmware-adjacent behavior.

# Ownership

- `test_main.cpp` registers all test suites.
- `tests/` owns individual test suites.
- `utils/` owns test helpers.

# Local Contracts

- Add new test suites to `test_main.cpp` so PlatformIO runs them.
- Keep tests deterministic and hardware-independent.
- Prefer testing shared logic in `lib/Common` or isolated firmware modules rather than depending on live MCU peripherals.

# Work Guidance

- Name test entry functions consistently as `runAll...Tests()`.
- Keep regression tests close to the behavior changed.

# Verification

- Run `pio test -e test` after modifying tests or tested logic.

# Child DOX Index

- `tests/AGENTS.md` - individual Unity test suites.
- `utils/AGENTS.md` - test helper utilities.
