# Purpose

- Own the standalone scales calibration sketch and matching calibration display binaries.

# Ownership

- `scales-calibration.cpp` is independent calibration firmware.
- `*.tft` files are calibration display artifacts.

# Local Contracts

- This fork's primary firmware target is no-scales; do not reintroduce scale runtime dependencies into the main build from here.
- Keep calibration assets isolated from the primary `src/` firmware unless explicitly requested.

# Work Guidance

- Treat calibration code as a hardware utility with separate flashing and display requirements.

# Verification

- No automated calibration verification exists yet.

# Child DOX Index

- No child DOX files.
