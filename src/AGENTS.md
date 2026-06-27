# Purpose

- Own the primary STM32 Blackpill firmware for this no-scales Gaggiuino fork.

# Ownership

- `gaggiuino.ino` and root headers/source own boot, loop orchestration, pin setup, logging, and high-level machine flow.
- `eeprom_data/` owns persisted settings, defaults, version metadata, and legacy migrations.
- `functional/` owns brew, steam, descale, and shot workflow logic.
- `lcd/` owns Nextion/TJC display communication.
- `peripherals/` owns hardware drivers and sensor/actuator integration.

# Local Contracts

- This fork targets Blackpill PCB, `SINGLE_BOARD`, `MAX31855`, 50 Hz baseline, and no runtime scales hardware.
- Do not re-enable scale runtime paths in the primary firmware without updating README, build filters, defaults, tests, and DOX.
- Keep `platformio.ini` build flags, source filters, and firmware assumptions in sync.
- EEPROM layout/version changes must preserve migration intent and update tests/defaults when behavior changes.

# Work Guidance

- Keep loop work non-blocking except where hardware startup explicitly requires sequencing.
- Prefer deterministic controller behavior and explicit safety fallbacks for heater, pump, valve, and relay logic.
- Follow existing C++ style: 2-space indentation, same-line braces, and concise embedded-friendly code.

# Verification

- Run `pio run -e all-pcb-stlink` for firmware build changes.
- Run `pio test -e test` for logic, controller, profile, EEPROM/default, or serialization changes.

# Child DOX Index

- `eeprom_data/AGENTS.md` - EEPROM schemas, defaults, metadata, and legacy versions.
- `functional/AGENTS.md` - brew/descale workflow and predictive shot logic.
- `lcd/AGENTS.md` - LCD communication and display state handling.
- `peripherals/AGENTS.md` - hardware drivers and sensor/actuator integration.
