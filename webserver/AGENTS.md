# Purpose

- Own ESP-side webserver firmware, filesystem upload tooling, and the embedded web interface package.

# Ownership

- `src/` owns ESP firmware for WiFi, filesystem, API routes, WebSocket, BLE scales bridge, and STM communication.
- `web-interface/` owns the React/Vite frontend built into `webserver/data`.
- `replace_fs.py`, `mklittlefs`, and related binaries/scripts support filesystem image work.

# Local Contracts

- Keep API/WebSocket payload contracts aligned across `webserver/src`, `lib/Common`, and `web-interface/src`.
- The primary STM firmware is no-scales; ESP BLE scale support here must not imply main firmware scale runtime support unless that contract changes.
- Generated web build output belongs in `webserver/data` when produced by `npm run build`; do not hand-edit generated bundle files.

# Work Guidance

- Prefer current maintained ESP/Arduino libraries when changing webserver dependencies.
- Keep local network target changes in `web-interface/vite.config.ts` out of durable defaults unless intentionally updating project behavior.

# Verification

- For ESP firmware changes, run the relevant PlatformIO webserver build/upload command if configured; otherwise state the missing environment.
- For frontend changes, run `npm run build` in `webserver/web-interface`.

# Child DOX Index

- `src/AGENTS.md` - ESP firmware source for WiFi, filesystem, API, WebSocket, STM comms, BLE scales, and logging.
- `web-interface/AGENTS.md` - React/Vite frontend source and package metadata.
