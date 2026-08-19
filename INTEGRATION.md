# Integrating Orbit Wallpaper Engine

The renderer accepts newline-terminated commands through:

```text
${XDG_CACHE_HOME:-$HOME/.cache}/orbit-wallpaper-engine/control
```

Supported commands are `intro`, `exit`, and `palette`.

## Safety

This FIFO is an optional visual-integration interface, not a session-management interface. A blocking shell write to an existing FIFO can wait indefinitely when no renderer is reading it. Never put an unbounded FIFO write in the critical path for logout, shutdown, lock, or another important action.

See `examples/generic/signal-wallpaper` for a timeout-bounded helper and `examples/hyprland/` for a reference Hyprland integration.

## Compatibility

The former `~/.cache/ps3-wave-wallpaper/control` path is obsolete.
