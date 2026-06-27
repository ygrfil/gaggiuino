# Purpose

- Own the React/Vite web interface served by the ESP filesystem.

# Ownership

- `src/` owns application pages, components, models, theme, API clients, charts, WiFi UI, and assets.
- `package.json` and lockfile own frontend dependency and script contracts.
- `vite.config.ts` owns dev server proxy and production output to `../data`.

# Local Contracts

- Keep frontend API/WebSocket models aligned with `webserver/src` and shared MCU payloads.
- Build output is generated into `webserver/data`; edit source files, not generated bundles.
- Preserve the configured dev server port `3000` unless updating README and workflows.

# Work Guidance

- Use maintained React/Vite/MUI patterns when touching dependencies or architecture.
- Keep UI changes practical for embedded-device administration: clear states, responsive layout, and readable diagnostics.
- Prefer existing component/theme patterns before adding new styling systems.

# Verification

- Run `npm run build` in this folder after source or dependency changes.
- Run `npm run lint` for JavaScript/TypeScript/React code changes when dependencies are installed.
- `npm test` starts Jest in watch mode; use an explicit non-watch invocation if adding automated CI later.

# Child DOX Index

- `src/AGENTS.md` - React application source, components, models, API clients, charts, pages, and assets.
