# Purpose

- Own firmware hardware integration for sensors, actuators, communication, and safety peripherals.

# Ownership

- Pump, pressure, temperature, thermocouple, LED, TOF, watchdog, ESP comms, and bus reset code live here.
- Scale source files are present but excluded from the primary no-scales build by `platformio.ini`.

# Local Contracts

- Preserve primary build exclusion of runtime scales unless the no-scales fork contract changes.
- Hardware driver changes must keep actuator fail-safe behavior explicit for pump, heater, valve, and relays.
- Sensor readings should keep units and smoothing assumptions traceable to downstream controllers.

# Work Guidance

- Avoid blocking I/O in hot loop paths.
- Keep initialization order compatible with `src/gaggiuino.ino`.

# Verification

- Run `pio run -e all-pcb-stlink` after peripheral integration changes.
- Run `pio test -e test` when behavior is covered by native logic tests.

# Child DOX Index

- No child DOX files.
