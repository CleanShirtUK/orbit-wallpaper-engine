# Wallpaper Control API

`orbit-wallpaper-control` is the frontend-neutral API for Orbit Wallpaper
Engine. Responses are JSON and include `ok` and `api_version: 1`.

## Commands

```text
orbit-wallpaper-control status --json
orbit-wallpaper-control intro [--id ID]
orbit-wallpaper-control exit [--id ID]
orbit-wallpaper-control palette [--id ID]
orbit-wallpaper-control config get --json
orbit-wallpaper-control config set KEY VALUE [KEY VALUE ...]
orbit-wallpaper-control config apply
orbit-wallpaper-control restart [--intro]
orbit-wallpaper-control shader list-installed --json
orbit-wallpaper-control shader catalogue [--refresh] --json
orbit-wallpaper-control shader inspect ID --json
orbit-wallpaper-control shader install ID
orbit-wallpaper-control shader apply ID [--intro]
```

The adapter owns the control and events FIFOs. Existing raw commands remain
supported for compatibility:

```text
intro [id]
exit [id]
palette [id]
```

FIFO writes are bounded. Commands wait for correlated `accepted` and
`completed` events, with operation-specific timeouts derived from the configured
intro/exit duration plus a small margin. A timeout or unavailable renderer
returns an error such as:

```json
{
  "ok": false,
  "api_version": 1,
  "error": {
    "code": "command_completion_timeout",
    "message": "Timed out waiting for command completion"
  }
}
```

## Configuration

The canonical configuration remains:

```text
~/.config/orbit-wallpaper-engine/config
```

`config set` validates ordinary restart-required settings and records them as
pending state without changing the canonical file. `config apply` exits the
current animation, atomically writes the pending candidate to the canonical
file, restarts the renderer, waits for readiness, and replays the intro. If
any apply, restart, or health step fails, the previous configuration is
restored and renderer recovery is attempted. Startup-only and internal
settings are intentionally rejected by the normal configuration surface.
This remains restart-required apply, not true live apply.

## Status

Status normalizes service state, PID, renderer readiness, output/surface
counts, active shader, animation, palette source, pending changes, and the
last adapter command. Raw EGL, GL, and Wayland implementation details are not
part of the API.
