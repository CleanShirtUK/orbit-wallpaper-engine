# Changelog

All notable changes to Orbit Wallpaper Engine will be documented here.

The project follows semantic versioning where practical.

## [0.1.2] - 2026-08-26

v0.1.2 is the Polish & Release Hardening milestone. It does not provide true
live apply; ordinary configuration changes still restart the renderer.

### Added

- Frontend-neutral `orbit-wallpaper-control` API with structured status,
  correlated animation commands, configuration staging, restart, and shader
  operations.
- Safe shader installation and application with licence/capability checks,
  blacklist enforcement, rollback, and previous/next installed-shader cycling.
- Wayland compatibility probing with compatible, unsupported, and cannot-verify
  outcomes, including permanent incompatibility status 78 handling.
- Bounded Hyprland login and lock/unlock reference transitions.
- Minimal optional Noctalia integration consisting of the palette hook and an
  icon-only launcher for the canonical Settings application.

### Improved

- Canonical Settings UI now uses the control API, has a scrollable content area,
  clearer action hierarchy, reachable narrow-window controls, and preserved
  shader-browser selection and scroll position across refresh/apply.
- Restart-required Apply now exits the current wallpaper, restarts with the
  staged configuration, waits for readiness, and replays the intro animation.
- Apply failure restores the previous configuration and attempts renderer
  recovery instead of silently leaving the desktop without a working state.
- Service reload now uses the bounded control API and animation timeouts follow
  configured intro/exit durations.
- Steam/Hyprland integration reconnects across compositor instance changes and
  treats unavailable sessions as recoverable.
- Installation modes now distinguish canonical GUI, optional integrations, and
  headless/core-only installation while retaining compatibility aliases.

### Fixed

- Boolean startup settings no longer treat the literal value `0` as enabled.
- Renderer readiness is not reported until all active outputs have valid EGL
  surfaces.
- Output removal, layer closure, hotplug slot reuse, monitor geometry changes,
  and mode changes no longer necessarily terminate or strand the entire
  renderer; affected surfaces are retired and recreated.
- Settings Apply no longer clears dirty state before successful process exit or
  starts the follow-up operation after a failed staging response.
- Settings staging no longer mutates the canonical configuration before Apply;
  successful Apply persists the candidate atomically before restart.
- Hyprland/Noctalia lock requests now use the active Hypridle boundary with
  bounded wallpaper exit and unlock intro handling.
- Narrow action rows wrap without clipping, and the standalone Settings window
  now opens at a 900x760 default size for the current content.
- Shader-browser filtering no longer leaves an invisible selection active, and
  browser delegates remain inside their list margins.
- Uninstall removes the optional Noctalia plugin instead of leaving stale files.

### Deferred

- True runtime/live parameter apply is reserved for v0.2.0.
- Audio/audio-reactive shaders and additional shader inputs are reserved for
  v0.3.0.
- GNOME/Mutter backend support remains future investigation with no promised
  implementation date.

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
