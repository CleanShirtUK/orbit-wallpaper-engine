# Orbit Wallpaper Engine

**Release status:** pre-1.0 / initial public release candidate. Interfaces may still change before 1.0.

A lightweight Wayland shader wallpaper renderer with a standalone Quickshell settings UI, cross-monitor rendering, animated desktop transitions, palette mapping, performance controls, and an optional shader browser.

Orbit Wallpaper Engine started as the wallpaper renderer for the Orbit desktop shell, but is designed to run independently.

## Features

- GLSL fragment shader wallpapers on Wayland
- Cross-monitor canvas coordinates
- Shadertoy-style compatibility for common uniforms and `mainImage`
- Intro / exit animation control
- Runtime palette mapping for shaders that do not provide their own palette uniforms
- Configurable fallback colours
- Target FPS, render scale, shader speed, brightness and resource-governor controls
- Standalone Quickshell settings application
- Optional shader browser backed by the KDE Shader Wallpaper catalogue
- Hyprland Steam game hook that can hide the wallpaper while a game is open
- User-level systemd service
- Safe shader apply with renderer rollback when a downloaded shader fails

## Requirements

The renderer requires:

- Wayland
- EGL / OpenGL ES 2
- libpng
- a compositor supporting `wlr-layer-shell`

The graphical settings application additionally requires [Quickshell](https://quickshell.org/).

Hyprland is only required for the optional Steam window hook.

## Install

Clone the repository and run:

```bash
./install.sh --check
./install.sh
```

On supported distributions, missing build dependencies can be installed with:

```bash
./install.sh --install-deps
```

The installer is user-scoped. It installs the renderer under `~/.local`, creates a user systemd service, and preserves an existing config and user shader directory during upgrades.

To install without the Steam hook:

```bash
./install.sh --no-steam-hooks
```

To install without the graphical settings launcher:

```bash
./install.sh --no-settings
```

## Settings

Launch:

```bash
orbit-wallpaper-settings
```

The renderer itself does not require Quickshell. Without the GUI, edit:

```text
~/.config/orbit-wallpaper-engine/config
```

User-installed shaders live in:

```text
~/.config/orbit-wallpaper-engine/shaders/
```

The built-in default shader is `wave.frag`.

## Palette

Orbit Wallpaper Engine ships with configurable fallback colours:

```ini
ORBIT_WALLPAPER_PRIMARY=#7AA2F7
ORBIT_WALLPAPER_SECONDARY=#BB9AF7
ORBIT_WALLPAPER_SURFACE=#24283B
ORBIT_WALLPAPER_ERROR=#F7768E
```

By default these colours are used directly.

A desktop shell or theme integration can provide a live palette file:

```ini
ORBIT_WALLPAPER_FOLLOW_SYSTEM_PALETTE=1
ORBIT_WALLPAPER_PALETTE_FILE=/absolute/path/to/palette
```

The renderer is intentionally not coupled to a particular shell or palette provider.

## Startup behaviour

The standalone/default configuration starts visible and plays the intro animation automatically.

Shell integrations may opt into deferred startup:

```ini
ORBIT_WALLPAPER_SKIP_INTRO=1
ORBIT_WALLPAPER_START_HIDDEN=1
```

and later send `intro` through the renderer control FIFO.

## Shader compatibility

Orbit attempts to adapt common standalone `.frag` and Shadertoy-style shaders by supplying commonly-used uniforms and a `mainImage` wrapper where required.

Not every shader can work automatically. Audio inputs, texture channels and multipass buffers are currently treated as unsupported by the shader browser.

Manual `.frag` files can be placed in the user shader directory regardless of whether they appear in the online catalogue.

## Shader browser and attribution

The optional shader browser uses the catalogue maintained by **KDE Shader Wallpaper**:

https://github.com/y4my4my4m/kde-shader-wallpaper

Full credit to the KDE Shader Wallpaper project and its contributors for the upstream shader collection and the work involved in curating it.

Orbit Wallpaper Engine is an independent project and is not affiliated with KDE or the KDE Shader Wallpaper project.

Catalogue shaders are downloaded on demand; they are not bundled with Orbit Wallpaper Engine. Licence and attribution information shown by Orbit is derived from available upstream metadata and shader source comments. See [SHADER_POLICY.md](SHADER_POLICY.md) for the project's handling of missing licence information and removal requests.

## Uninstall

```bash
./uninstall.sh
```

This preserves user config and shaders.

To remove user data too:

```bash
./uninstall.sh --purge
```

## Credits

- [KDE Shader Wallpaper](https://github.com/y4my4my4m/kde-shader-wallpaper) — upstream shader catalogue and major inspiration for shader-library functionality.
- [Quickshell](https://quickshell.org/) — QtQuick shell toolkit used for the optional graphical settings application.
- [Wayland / wayland-protocols](https://gitlab.freedesktop.org/wayland/wayland-protocols) — protocol definitions used by the renderer.
- `wlr-layer-shell` / wlroots ecosystem — layer-shell protocol used to place the renderer on the desktop background layer.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for licensing details.

## AI disclosure

AI assistance was substantial during development. The project concept, behaviour, visual direction, acceptance decisions and hands-on regression testing were human-led; substantial portions of implementation, debugging, migration tooling and documentation were drafted or revised with OpenAI ChatGPT under that direction.

See [AI_DISCLOSURE.md](AI_DISCLOSURE.md) for the full disclosure.


## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Security reports should follow [SECURITY.md](SECURITY.md).

## Licence

Orbit Wallpaper Engine project code and the bundled `wave.frag` are released under **GPL-3.0-or-later**, except for files that carry their own third-party licence notices.

See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
