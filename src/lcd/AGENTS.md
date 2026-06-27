# Purpose

- Own firmware-side LCD communication and display state handling.

# Ownership

- `lcd.h` defines LCD-facing contracts.
- `nextion.cpp` implements Nextion/TJC communication and page/value synchronization.

# Local Contracts

- Keep LCD page IDs, command names, and upload payloads aligned with `lcd-hmi/` source assets and tracked TFT files.
- User activity detection must remain compatible with inactivity standby behavior.
- Avoid display updates that block the firmware loop unnecessarily.

# Work Guidance

- Keep UI protocol changes small and mirrored in display assets or web/ESP consumers when relevant.

# Verification

- Run `pio run -e all-pcb-stlink` after LCD firmware changes.
- No automated HMI round-trip verification exists yet.

# Child DOX Index

- No child DOX files.
