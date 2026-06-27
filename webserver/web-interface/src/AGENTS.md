# Purpose

- Own React application source for the embedded browser UI.

# Ownership

- `pages/` owns routed views and page-level composition.
- `components/` owns reusable inputs, charts, WiFi controls, app bar, theme, loader, table, and log components.
- `models/` owns API/profile data model helpers and prop types.
- Root app files wire React, routing, setup, and static assets.

# Local Contracts

- Keep API client paths and payload assumptions aligned with `webserver/src/server`.
- Keep profile structures aligned with firmware/shared profile serialization.
- Do not edit generated build output in `webserver/data` to make UI changes.

# Work Guidance

- Prefer existing MUI/theme and component patterns before adding new dependencies.
- Keep controls usable on small screens served from an embedded device.
- Avoid decorative UI changes that reduce readability of live machine state.

# Verification

- Run `npm run build` from `webserver/web-interface` after source changes.
- Run `npm run lint` when dependencies are installed and lint scope is relevant.

# Child DOX Index

- No child DOX files.
