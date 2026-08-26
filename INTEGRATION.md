# Integrating Orbit Wallpaper Engine

The renderer accepts newline-terminated commands through:

```text
${XDG_CACHE_HOME:-$HOME/.cache}/orbit-wallpaper-engine/control
```

Supported commands are `intro`, `exit`, and `palette`.

Commands may include an identifier, for example `intro login-1`. The renderer
publishes readiness in `${XDG_CACHE_HOME:-$HOME/.cache}/orbit-wallpaper-engine/status`
and non-blocking command events in the `events` FIFO in the same directory.
Events contain `command`, `id`, `phase=accepted|completed`, and surface counts.
`phase=completed` is emitted only after the requested animation reaches its
terminal renderer state. The status file reports `readiness=ready` only after
all discovered outputs have configured EGL layer surfaces.

## Safety

This FIFO is an optional visual-integration interface, not a session-management interface. A blocking shell write to an existing FIFO can wait indefinitely when no renderer is reading it. Never put an unbounded FIFO write in the critical path for logout, shutdown, lock, or another important action.

See `examples/generic/signal-wallpaper` for a timeout-bounded helper and `examples/hyprland/` for a reference Hyprland integration.

## Compatibility

The former `~/.cache/ps3-wave-wallpaper/control` path is obsolete.

## Control API

New frontends should use `orbit-wallpaper-control` rather than opening the
FIFOs directly. It provides API version 1 JSON responses for status, intro,
exit, palette, configuration, restart, and shader management while preserving
the raw FIFO commands for existing integrations.

The adapter uses bounded FIFO writes and command correlation IDs. A missing or
unresponsive renderer returns a structured error instead of blocking the
caller. See `CONTROL_API.md` for the command and response contract.

## Noctalia Integration

The renderer and `orbit-wallpaper-control` are the common core for all
installation modes. The standalone Wallpaper Engine Settings application is
the single canonical GUI. The optional Noctalia integration is only a custom
launcher button; it does not provide a second settings or control frontend.

The Noctalia button launches the canonical application with a Hyprland
one-shot rule for a floating top-right window:

```sh
hyprctl dispatch 'hl.dsp.exec_cmd("orbit-wallpaper-settings", { float = true, size = { 560, 760 }, move = { "monitor_w-560-20", 45 } })'
```

Normal desktop-entry and application-launcher invocations remain unchanged.
The one-shot rule is used only by the Noctalia button.

Orbit remains the actual wallpaper owner, while Noctalia's built-in static
wallpaper API remains disabled for this setup. The existing hook is preserved
unchanged:

```toml
[hooks]
colors_changed = "timeout 1s ~/.config/hypr/scripts/wallpaper-animation palette || true"
```

For session transitions, use the bounded reference files under
`examples/hyprland/`. `animate-login` waits for renderer readiness before
replaying `intro`. The Hypridle configuration routes the normal `loginctl`
lock request through `start-hyprlock`, which sends `exit` before Hyprlock;
Hypridle's bounded `unlock_cmd` replays `intro` after unlock. Missing or
unresponsive renderer commands are non-fatal to the session action.
