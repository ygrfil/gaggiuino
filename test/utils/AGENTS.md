# Purpose

- Own helper utilities shared by native Unity tests.

# Ownership

- Test helper headers and implementation files live here.

# Local Contracts

- Keep helpers deterministic and free of hidden global state unless tests reset it explicitly.
- Avoid making helpers depend on production hardware libraries.

# Work Guidance

- Prefer small helper functions that make tests easier to read without hiding the behavior under test.

# Verification

- Run `pio test -e test` after helper changes.

# Child DOX Index

- No child DOX files.
