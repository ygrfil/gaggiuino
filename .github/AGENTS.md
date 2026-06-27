# Purpose

- Own GitHub-facing repository automation and contribution metadata.
- Cover workflows, issue templates, and CODEOWNERS.

# Ownership

- `.github/workflows/` owns CI definitions for firmware compilation.
- `.github/ISSUE_TEMPLATE/` owns issue intake forms.
- `.github/CODEOWNERS` owns GitHub review ownership metadata.

# Local Contracts

- Keep workflow branch names, PlatformIO environments, and build commands aligned with `platformio.ini`.
- Do not add secrets, tokens, machine-local paths, or user-specific settings to workflow files.
- Treat workflow changes as repository behavior changes and update this doc when CI responsibilities change.

# Work Guidance

- Prefer maintained GitHub Actions versions and current action inputs.
- Keep CI focused on reproducible repository checks rather than local hardware flashing.

# Verification

- For workflow changes, validate YAML syntax and check that referenced commands exist in the repo.
- When possible, run the equivalent local command before relying on CI.

# Child DOX Index

- No child DOX files.
