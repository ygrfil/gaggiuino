# Purpose

- Own editable HMI source files for Nextion/TJC display projects.

# Ownership

- `*.HMI` files are source assets for display layouts and generated TFT binaries.

# Local Contracts

- Keep HMI source changes aligned with firmware LCD protocol changes in `src/lcd/`.
- If generated TFT files are updated from HMI changes, document or include the matching source update.
- Do not edit binary display outputs as a substitute for updating HMI sources.

# Work Guidance

- Keep page IDs, component names, and command expectations compatible with firmware LCD handling.

# Verification

- No automated HMI verification exists yet.

# Child DOX Index

- No child DOX files.
