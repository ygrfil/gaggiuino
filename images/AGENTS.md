# Purpose

- Own static image assets used by project documentation and UI references.

# Ownership

- Image files in this folder are durable visual assets, not generated build output.

# Local Contracts

- Preserve filenames referenced by Markdown, web UI code, or documentation unless all callers are updated together.
- Avoid replacing transparent logo/icon assets with opaque or cropped versions.
- Do not commit large raw design sources here unless they are intentionally part of the repo.

# Work Guidance

- Prefer compressed web-friendly formats for documentation/UI imagery.
- Keep visual changes inspectable in the context where the asset is used.

# Verification

- Check references with `rg '<filename>'` after renaming or deleting an asset.

# Child DOX Index

- No child DOX files.
