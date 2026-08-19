# Migration to Orbit Wallpaper Engine

The public application identity is changing from `ps3-wave-wallpaper` to
`orbit-wallpaper-engine`.

The new configuration namespace is `ORBIT_WALLPAPER_*`. During the transition,
the renderer still accepts the former `PS3_WAVE_*` environment names when the
new equivalent is not present.

New user paths:

- `~/.config/orbit-wallpaper-engine/`
- `~/.local/share/orbit-wallpaper-engine/`
- `~/.cache/orbit-wallpaper-engine/`
- `~/.local/bin/orbit-wallpaper-engine`
- `orbit-wallpaper-engine.service`

Step 5A changes the source tree only. It does not replace the currently-running
legacy installation.

## Control FIFO

Integrations must use `${XDG_CACHE_HOME:-$HOME/.cache}/orbit-wallpaper-engine/control`. The former `~/.cache/ps3-wave-wallpaper/control` path is obsolete. Writes should be timeout-bounded; see `INTEGRATION.md`.

