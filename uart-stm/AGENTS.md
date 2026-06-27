# Purpose

- Own the standalone STM UART diagnostic sketch.

# Ownership

- `main.ino` is a utility sketch, separate from the main PlatformIO firmware target.

# Local Contracts

- Keep diagnostic protocol assumptions aligned with shared MCU communication structures when changed.
- Do not route production firmware behavior through this utility folder.

# Work Guidance

- Keep the sketch focused on UART diagnosis and easy hardware bring-up.

# Verification

- No automated verification exists yet.

# Child DOX Index

- No child DOX files.
