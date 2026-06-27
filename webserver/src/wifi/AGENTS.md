# Purpose

- Own ESP WiFi setup and connection behavior.

# Ownership

- `wifi_setup.*` handles access point/client setup and network connection flow.

# Local Contracts

- Do not commit real SSIDs, passwords, or local-only network assumptions.
- Keep WiFi behavior compatible with setup flows exposed by the web UI.

# Work Guidance

- Keep connection states observable through logs or API/UI surfaces without exposing secrets.

# Verification

- No dedicated automated WiFi verification exists yet.

# Child DOX Index

- No child DOX files.
