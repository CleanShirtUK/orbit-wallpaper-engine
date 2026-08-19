# Hyprland reference integration

These are reference examples, not required components. Orbit Wallpaper Engine does not manage the Hyprland session.

`wallpaper-animation` sends `intro`, `exit`, or `palette` to the renderer. `animate-shutdown` demonstrates one possible logind-managed logout flow; adapt the termination command to how Hyprland is launched on your system.

## Important design rule

Wallpaper animation is cosmetic and must never be required for logout, shutdown, locking, or another session-critical action. FIFO writes here are skipped when the FIFO is absent, timeout-bounded when there is no reader, and non-fatal to the logout example.

The control FIFO is `${XDG_CACHE_HOME:-$HOME/.cache}/orbit-wallpaper-engine/control`.

Standalone operation requires none of these scripts.
