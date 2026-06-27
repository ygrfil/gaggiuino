# Purpose

- Own repository helper scripts for PlatformIO source selection, version generation, and SD/filesystem packaging.

# Ownership

- `auto-version.py` generates firmware version metadata during PlatformIO builds.
- `custom-src-dir.py` customizes PlatformIO source directory behavior.
- `make-sd.sh` builds or packages SD/filesystem artifacts.

# Local Contracts

- Scripts invoked by `platformio.ini` are build-critical; preserve their command-line behavior and PlatformIO integration points.
- Avoid machine-specific absolute paths.
- Keep generated files out of version control unless the project already tracks them intentionally.

# Work Guidance

- Prefer standard-library Python or POSIX shell features unless a dependency is already part of the build environment.
- Keep script output deterministic enough for CI/build logs.

# Verification

- Run `pio run -e all-pcb-stlink` after changing PlatformIO pre-scripts.
- For packaging script changes, run the script or document why hardware/tooling prevented verification.

# Child DOX Index

- No child DOX files.
