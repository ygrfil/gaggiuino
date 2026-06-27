# Purpose

- Own ESP HTTP server behavior, API routes, static file serving, utilities, and WebSocket transport.

# Ownership

- `api/` owns REST/static-file handlers.
- `websocket/` owns live sensor, shot, and log updates to browser clients.
- `utils/` owns server helper functions.
- `server_setup.*` wires route registration and server startup.

# Local Contracts

- Keep route paths and WebSocket payload shapes compatible with `webserver/web-interface/src`.
- Do not log WiFi credentials or other secrets through remote logs or client responses.
- Static file behavior must remain compatible with the filesystem image produced from the web interface build.

# Work Guidance

- Keep handler code small and explicit about HTTP status/error behavior.
- Prefer centralized helpers for repeated response formatting.

# Verification

- No dedicated automated server verification exists yet.

# Child DOX Index

- No child DOX files.
