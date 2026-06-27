# Purpose

- Own mock implementations of Arduino and hardware libraries for native tests.

# Ownership

- Mock headers and sources stand in for hardware dependencies such as ADS1X15, HX711, NCP5623, PSM, EasyNextionLibrary, Wire, and watchdog APIs.

# Local Contracts

- Keep mock public APIs compatible with the production libraries referenced by firmware code.
- Do not add behavior that makes native tests pass while contradicting realistic call order or value semantics.

# Work Guidance

- Model only what tests need, with explicit defaults and reset behavior.

# Verification

- Run `pio test -e test` after mock changes.

# Child DOX Index

- No child DOX files.
