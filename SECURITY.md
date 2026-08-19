# Security Policy

## Supported versions

Until the first stable release, only the latest release is supported.

## Reporting a security issue

Please avoid filing a public issue for vulnerabilities that could materially compromise a user's desktop session, files or account.

Use GitHub's private vulnerability reporting feature when available for this repository. If private reporting is not available, open a minimal issue asking the maintainer for a private contact method without including exploit details.

## Scope

Security-sensitive areas include:

- shader downloads and local shader installation;
- renderer rollback after shader failure;
- file writes under user config/cache/data directories;
- systemd user services;
- Hyprland IPC handling;
- external catalogue metadata;
- installer and uninstaller behaviour.

Third-party shader copyright or attribution concerns should use the separate shader-removal issue template rather than this security process.
