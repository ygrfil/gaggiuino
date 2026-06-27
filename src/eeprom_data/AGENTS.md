# Purpose

- Own persisted configuration structures, default profiles, EEPROM metadata, and legacy schema files.

# Ownership

- Current EEPROM contract lives in `eeprom_data.h`, `eeprom_data.cpp`, `eeprom_metadata.h`, and `default_profiles.h`.
- `legacy/` preserves older structures needed for migration/reset behavior.

# Local Contracts

- Any incompatible persisted settings change requires an intentional EEPROM version decision.
- Default profile changes must be validated against serializer/profile tests.
- Do not delete legacy schema files unless migration/reset behavior no longer references them.

# Work Guidance

- Keep defaults aligned with the no-scales Blackpill fork described in `README.md`.
- Use explicit units and conservative machine-safe defaults.

# Verification

- Run `pio test -e test` after default profile, metadata, or serialization-related changes.
- Run `pio run -e all-pcb-stlink` after structural EEPROM changes.

# Child DOX Index

- `legacy/AGENTS.md` - historical EEPROM schema definitions used for migration/reset compatibility.
