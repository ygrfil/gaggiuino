# Purpose

- Own mock libraries used by the native PlatformIO test environment.

# Ownership

- `mocks/` provides stand-ins for Arduino and hardware-specific dependencies.

# Local Contracts

- Mocks should model only the behavior needed by tests; avoid pretending to validate hardware timing or electrical behavior.
- Keep mock APIs compatible with the production libraries used by firmware code.

# Work Guidance

- Prefer simple observable state over complex mock frameworks.

# Verification

- Run `pio test -e test` after mock changes.

# Child DOX Index

- `mocks/AGENTS.md` - mocked hardware/Arduino libraries for native tests.
