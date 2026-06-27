# Purpose

- Own ESP firmware source for serving the UI, WiFi setup, API routes, WebSocket updates, BLE scale bridge, and STM communication.

# Ownership

- `main.cpp` wires startup and callback flow.
- `server/` owns HTTP API and WebSocket behavior.
- `stm_comms/` owns STM serial protocol integration.
- `wifi/`, `filesystem/`, `scales/`, and `log/` own their named subsystems.

# Local Contracts

- Keep WebSocket/API schemas compatible with `webserver/web-interface/src` consumers.
- Keep STM message parsing compatible with shared structures from `lib/Common`.
- Avoid blocking startup or loop behavior beyond explicit setup tasks.

# Work Guidance

- Keep task ownership and FreeRTOS lifecycle decisions obvious at call sites.
- Keep logging useful for remote diagnostics without leaking credentials.

# Verification

- No dedicated PlatformIO webserver environment is currently declared in root `platformio.ini`.
- When a webserver build environment is added, document the command here.

# Child DOX Index

- `server/AGENTS.md` - HTTP API, static-file handling, server utilities, and WebSocket routes.
- `stm_comms/AGENTS.md` - serial communication with STM firmware.
- `wifi/AGENTS.md` - WiFi setup and connection handling.
