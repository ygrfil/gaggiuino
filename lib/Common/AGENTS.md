# Purpose

- Own common C++ modules shared across firmware, tests, and ESP/STM communication boundaries.

# Ownership

- Controllers: PID and pressure control.
- Data/protocol models: MCU comms, measurements, sensors state, system state, profiling phases.
- Utilities: inactivity tracking and general helpers.

# Local Contracts

- Keep serialized message formats compatible across STM firmware, ESP webserver firmware, and web UI consumers.
- Controller changes must preserve deterministic behavior and be covered by native tests.
- Avoid introducing Arduino-only dependencies into modules that are exercised by native tests.

# Work Guidance

- Keep calculations explicit about units, ranges, and saturation behavior.
- Prefer plain structs and stable field names for communication payloads.

# Verification

- Run `pio test -e test` after changes to controllers, profiling, serializer, measurements, or state structures.

# Child DOX Index

- No child DOX files.
