# Changelog

All notable changes to Orbit Wallpaper Engine will be documented here.

The project follows semantic versioning where practical.

## [0.1.1] - 2026-08-19

### Added

- Documented the renderer control FIFO as an integration API.
- Added generic timeout-bounded renderer signalling example.
- Added a Hyprland reference integration for wallpaper session transitions.

### Changed

- Reference integrations treat wallpaper transitions as cosmetic and non-fatal so a missing renderer cannot block a session-critical action.
- Documented the obsolete pre-rename control FIFO path.

## [0.1.0] - 2026-08-19

Initial public release candidate.

### Added

- Wayland shader wallpaper renderer using wlr-layer-shell.
- Cross-monitor shader canvas.
- Common Shadertoy-style fragment shader compatibility.
- Built-in `wave.frag` shader.
- Configurable intro and exit animations.
- Runtime palette mapping and configurable fallback colours.
- Brightness, FPS, render-scale and shader-speed controls.
- Resource governor for reducing renderer load under pressure.
- Standalone Quickshell settings interface.
- Orbit shell integration through the shared settings component.
- Optional KDE Shader Wallpaper catalogue browser.
- Shader compatibility checks and safe renderer rollback.
- Runtime shader blacklist and GitHub removal-request workflow.
- Hyprland Steam-game watcher.
- User-level systemd services.
- Installer, uninstaller and upgrade-safe user configuration.
- Legacy `PS3_WAVE_*` environment compatibility for migration.
- Third-party notices, shader policy and explicit AI-development disclosure.
