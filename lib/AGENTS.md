# Purpose

- Own shared PlatformIO library code used by firmware and tests.

# Ownership

- `Common/` contains reusable controllers, protocol types, measurements, system state, profiling phases, and utility code.

# Local Contracts

- Shared code must stay independent of sketch-only globals unless the dependency is explicit and tested.
- Public headers in shared libraries define cross-module contracts; update tests and callers when changing them.
- Changes under `Common/` must follow `Common/AGENTS.md`.

# Work Guidance

- Prefer small, deterministic classes/functions that can run in the native test environment.
- Keep embedded constraints in mind: avoid heap-heavy designs, hidden blocking behavior, and unnecessary floating-point churn in hot paths.

# Verification

- Run `pio test -e test` for controller, serialization, or profiling changes.

# Child DOX Index

- `Common/AGENTS.md` - shared controller, measurement, protocol, and state modules.
