# Purpose

- Own individual Unity test suites for shared firmware logic.

# Ownership

- Each `test_*.cpp` file covers a specific behavior area such as PID, pump, pressure control, profiles, or inactivity.

# Local Contracts

- New suites must expose a runner and be included from `test/test_main.cpp`.
- Tests should avoid hardware-only dependencies and use mocks/helpers where needed.

# Work Guidance

- Keep assertions specific to externally visible behavior or stable calculation contracts.

# Verification

- Run `pio test -e test` after changes.

# Child DOX Index

- No child DOX files.
