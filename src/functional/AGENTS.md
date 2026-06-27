# Purpose

- Own high-level machine workflows such as brewing, descale, and predictive water/shot logic.

# Ownership

- Coffee workflow lives in `just_do_coffee.*`.
- Descale workflow lives in `descale.*`.
- Predictive no-scales estimation contracts live in `predictive_weight.h`.

# Local Contracts

- Preserve no-scales behavior: shot estimates are based on pumped-water flow, not physical scale readings.
- Workflow changes that affect heater, pump, valve, steam, or standby behavior must account for fail-safe states.
- Keep workflow logic compatible with LCD commands and system state transitions.

# Work Guidance

- Favor explicit state transitions over hidden side effects.
- Keep timing and thresholds easy to trace from sensor inputs to actuator outputs.

# Verification

- Run `pio test -e test` for testable workflow math/state changes.
- Run `pio run -e all-pcb-stlink` for firmware integration changes.

# Child DOX Index

- No child DOX files.
