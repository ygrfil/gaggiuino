# Purpose

- Own historical EEPROM schema headers retained for compatibility decisions.

# Ownership

- Each `eeprom_data_v*.h` file represents an older persisted layout.

# Local Contracts

- Do not mutate historical schema definitions casually; add new current schema work in the parent folder.
- Deleting a legacy schema requires confirming no migration/reset path references it.

# Work Guidance

- Keep old layouts readable and isolated from current defaults.

# Verification

- Run `pio test -e test` after changing legacy schema availability or references.

# Child DOX Index

- No child DOX files.
