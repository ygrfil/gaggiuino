# Purpose

- Own ESP-side serial communication with the STM firmware.

# Ownership

- `stm_comms.*` receives STM sensor/shot snapshots and forwards commands/events to higher-level ESP modules.

# Local Contracts

- Keep message structures and serialization aligned with `lib/Common/mcu_comms.*`.
- Changes that affect browser-visible payloads must be reflected in `webserver/src/server/` and `webserver/web-interface/src`.

# Work Guidance

- Keep parsing robust to partial or noisy serial data.
- Avoid blocking operations in communication callbacks.

# Verification

- No dedicated automated STM comms verification exists yet.

# Child DOX Index

- No child DOX files.
