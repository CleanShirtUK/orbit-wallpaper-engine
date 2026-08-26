#!/usr/bin/env bash
set -Eeuo pipefail

APP="orbit-wallpaper-engine"
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

PREFIX="${PREFIX:-$HOME/.local}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
CACHE_HOME="${XDG_CACHE_HOME:-$HOME/.cache}"

BINDIR="$PREFIX/bin"
LIBEXECDIR="$PREFIX/libexec/$APP"
APP_DATA="$DATA_HOME/$APP"
APP_CONFIG="$CONFIG_HOME/$APP"
SYSTEMD_DIR="$CONFIG_HOME/systemd/user"
APPLICATIONS_DIR="$DATA_HOME/applications"
NOCTALIA_PLUGIN_DIR="$DATA_HOME/noctalia/plugins/orbit-wallpaper-engine"

INSTALL_DEPS=0
ENABLE_STEAM=1
NO_START=0
CHECK_ONLY=0
FRONTEND=""
FRONTEND_EXPLICIT=0
NOCTALIA_INTEGRATION=0
INTEGRATION_EXPLICIT=0
NO_GUI=0
NO_SETTINGS_REQUESTED=0

usage() {
    cat <<EOF
Usage: ./install.sh [options]

Options:
  --install-deps     Offer to install missing build dependencies.
  --frontend MODE    Install standalone or none (headless).
  --integration NAME Enable an optional integration, currently noctalia.
  --no-gui           Backwards-compatible alias for --frontend none.
  --no-steam-hooks   Do not enable the Hyprland Steam game watcher.
  --no-settings      Do not install the graphical settings launcher.
  --no-start         Install files but do not enable/start services.
  --check            Check dependencies only; change nothing.
  -h, --help         Show this help.
EOF
}

while (($#)); do
    case "$1" in
        --install-deps) INSTALL_DEPS=1 ;;
        --frontend)
            (($# >= 2)) || { echo "--frontend requires standalone or none" >&2; exit 2; }
            ((FRONTEND_EXPLICIT == 0)) || { echo "Conflicting --frontend options." >&2; exit 2; }
            FRONTEND="$2"
            FRONTEND_EXPLICIT=1
            shift
            ;;
        --integration)
            (($# >= 2)) || { echo "--integration requires a name" >&2; exit 2; }
            ((INTEGRATION_EXPLICIT == 0)) || { echo "Conflicting --integration options." >&2; exit 2; }
            [[ "$2" == "noctalia" ]] || { echo "Invalid integration '$2'; choose noctalia." >&2; exit 2; }
            NOCTALIA_INTEGRATION=1
            INTEGRATION_EXPLICIT=1
            shift
            ;;
        --no-gui) NO_GUI=1 ;;
        --no-steam-hooks) ENABLE_STEAM=0 ;;
        --no-settings) NO_SETTINGS_REQUESTED=1 ;;
        --no-start) NO_START=1 ;;
        --check) CHECK_ONLY=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if (( FRONTEND_EXPLICIT )); then
    case "$FRONTEND" in
        standalone|none) ;;
        noctalia)
            echo "Compatibility alias: --frontend noctalia is now --frontend standalone --integration noctalia." >&2
            FRONTEND="standalone"
            NOCTALIA_INTEGRATION=1
            ;;
        *) echo "Invalid frontend '$FRONTEND'; choose standalone or none." >&2; exit 2 ;;
    esac
fi

if (( NO_GUI )) && (( FRONTEND_EXPLICIT )) && [[ "$FRONTEND" != "none" ]]; then
    echo "Conflicting options: --no-gui is an alias for --frontend none." >&2
    exit 2
fi

if (( NOCTALIA_INTEGRATION )) && (( NO_GUI || NO_SETTINGS_REQUESTED )); then
    echo "Conflicting options: the Noctalia integration requires the canonical settings GUI." >&2
    exit 2
fi
if (( NOCTALIA_INTEGRATION )) && (( FRONTEND_EXPLICIT )) && [[ "$FRONTEND" == "none" ]]; then
    echo "Conflicting options: the Noctalia integration requires --frontend standalone." >&2
    exit 2
fi

if (( NO_SETTINGS_REQUESTED )) && (( FRONTEND_EXPLICIT )) && [[ "$FRONTEND" != "none" ]]; then
    echo "Conflicting options: --frontend $FRONTEND installs a GUI; do not combine it with --no-settings." >&2
    exit 2
fi

choose_frontend() {
    if (( FRONTEND_EXPLICIT )); then
        return
    fi
    if (( NO_GUI || NO_SETTINGS_REQUESTED )); then
        FRONTEND="none"
        return
    fi
    if [[ -t 0 && -t 1 ]]; then
        cat <<'EOF'

Which core installation would you like?

1. Standalone
   Install the standalone Wallpaper Settings application.

2. None / headless
   Install the renderer and control API only.

Choose [1]:
EOF
        local choice
        read -r choice
        case "${choice:-1}" in
            1) FRONTEND="standalone" ;;
            2) FRONTEND="none" ;;
            *) echo "Invalid frontend selection." >&2; exit 2 ;;
        esac
    else
        FRONTEND="standalone"
    fi
}

choose_frontend

ENABLE_SETTINGS=0
[[ "$FRONTEND" == "standalone" ]] && ENABLE_SETTINGS=1

say() { printf '\n==> %s\n' "$*"; }
have() { command -v "$1" >/dev/null 2>&1; }

required_commands=(cc make pkg-config systemctl python3)
missing_commands=()
for c in "${required_commands[@]}"; do
    have "$c" || missing_commands+=("$c")
done

required_pc=(wayland-client egl glesv2 libpng)
missing_pc=()
if have pkg-config; then
    for p in "${required_pc[@]}"; do
        pkg-config --exists "$p" || missing_pc+=("$p")
    done
else
    missing_pc=("${required_pc[@]}")
fi

dependency_hint() {
    if have dnf; then
        echo "Fedora:"
        echo "  sudo dnf install gcc make pkgconf-pkg-config wayland-devel mesa-libEGL-devel mesa-libGLES-devel libpng-devel"
    elif have pacman; then
        echo "Arch:"
        echo "  sudo pacman -S --needed base-devel pkgconf wayland mesa libpng"
    elif have apt-get; then
        echo "Debian/Ubuntu:"
        echo "  sudo apt-get install build-essential pkg-config libwayland-dev libegl1-mesa-dev libgles2-mesa-dev libpng-dev"
    else
        echo "Install a C compiler, make, pkg-config, Wayland/EGL/GLES development files, and libpng development files."
    fi
}

install_dependencies() {
    if have dnf; then
        sudo dnf install -y gcc make pkgconf-pkg-config wayland-devel mesa-libEGL-devel mesa-libGLES-devel libpng-devel
    elif have pacman; then
        sudo pacman -S --needed base-devel pkgconf wayland mesa libpng
    elif have apt-get; then
        sudo apt-get update
        sudo apt-get install -y build-essential pkg-config libwayland-dev libegl1-mesa-dev libgles2-mesa-dev libpng-dev
    else
        echo "Automatic dependency installation is not supported on this distribution." >&2
        dependency_hint >&2
        exit 1
    fi
}

if ((${#missing_commands[@]} || ${#missing_pc[@]})); then
    echo "Missing build requirements."
    ((${#missing_commands[@]})) && printf 'Commands: %s\n' "${missing_commands[*]}"
    ((${#missing_pc[@]})) && printf 'pkg-config modules: %s\n' "${missing_pc[*]}"
    dependency_hint

    if (( INSTALL_DEPS )); then
        install_dependencies
    else
        echo
        echo "Re-run with --install-deps to install supported distro packages automatically."
        exit 1
    fi
fi

wayland_compatibility() {
    local probe_output probe_state missing
    if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
        echo "Wayland compatibility: CANNOT VERIFY (WAYLAND_DISPLAY is not set)"
        echo "The installation can continue, but runtime compatibility must be checked from the graphical session."
        return 0
    fi

    if [[ -n "${ORBIT_WALLPAPER_WAYLAND_PROBE_OUTPUT:-}" ]]; then
        probe_output="$ORBIT_WALLPAPER_WAYLAND_PROBE_OUTPUT"
    elif [[ -n "${ORBIT_WALLPAPER_WAYLAND_PROBE:-}" ]]; then
        if ! probe_output="$($ORBIT_WALLPAPER_WAYLAND_PROBE 2>/dev/null)"; then
            echo "Wayland compatibility: CANNOT VERIFY (probe failed)"
            return 0
        fi
    else
        local probe
        local -a probe_cflags probe_libs
        probe="${TMPDIR:-/tmp}/orbit-wallpaper-wayland-check.$$"
        read -r -a probe_cflags <<< "$(pkg-config --cflags wayland-client)"
        read -r -a probe_libs <<< "$(pkg-config --libs wayland-client)"
        if ! cc "${probe_cflags[@]}" \
                "$ROOT/tools/orbit-wayland-capabilities.c" \
                -o "$probe" "${probe_libs[@]}"; then
            rm -f "$probe"
            echo "Wayland compatibility: CANNOT VERIFY (could not build the capability probe)"
            return 0
        fi
        if ! probe_output="$($probe 2>/dev/null)"; then
            rm -f "$probe"
            echo "Wayland compatibility: CANNOT VERIFY (probe failed)"
            return 0
        fi
        rm -f "$probe"
    fi

    probe_state="$(python3 -c 'import json, sys; print(json.loads(sys.stdin.read()).get("state", "CANNOT_VERIFY"))' <<<"$probe_output" 2>/dev/null || printf '%s' CANNOT_VERIFY)"
    case "$probe_state" in
        COMPATIBLE)
            echo "Wayland compatibility: COMPATIBLE (required globals detected)"
            ;;
        UNSUPPORTED)
            missing="$(python3 -c 'import json, sys; print(", ".join(json.loads(sys.stdin.read()).get("missing", [])))' <<<"$probe_output" 2>/dev/null || printf '%s' 'unknown required capability')"
            cat >&2 <<EOF
Wallpaper Engine cannot run in the current Wayland session.

Missing required Wayland protocol/global:
  $missing

The current compositor does not expose a protocol required by the
Wallpaper Engine renderer.

Renderer installation/startup has not been performed.
EOF
            return 1
            ;;
        *)
            echo "Wayland compatibility: CANNOT VERIFY (the active session could not be inspected)"
            ;;
    esac
}

say "Build dependencies are available"
wayland_compatibility

if (( CHECK_ONLY )); then
    if [[ "$FRONTEND" == "standalone" ]]; then
        if have quickshell || [[ -x "$HOME/.local/opt/quickshell/bin/quickshell" ]]; then
            echo "Settings GUI: available (Quickshell found)"
        else
            echo "Settings GUI: selected (Quickshell not found; runtime UI unavailable)"
        fi
    else
        echo "Frontend: none (headless)"
    fi
    if have hyprctl; then
        echo "Hyprland tools: available"
    else
        echo "Hyprland tools: not found (Steam hooks will wait for/detect Hyprland at runtime)"
    fi
    exit 0
fi

say "Building $APP"
make -C "$ROOT" clean
make -C "$ROOT"

[[ -x "$ROOT/$APP" ]] || {
    echo "Build completed without creating $ROOT/$APP" >&2
    exit 1
}

install_core() {
    say "Creating common-core installation directories"
    install -d \
        "$BINDIR" \
        "$LIBEXECDIR" \
        "$APP_DATA" \
        "$APP_CONFIG/shaders" \
        "$CACHE_HOME/$APP" \
        "$SYSTEMD_DIR" \
        "$APPLICATIONS_DIR"

    say "Installing common renderer and backend"
    install -m 0755 "$ROOT/$APP" "$BINDIR/$APP"
    install -m 0755 "$ROOT/tools/orbit-wallpaper-helper" "$BINDIR/orbit-wallpaper-helper"
    install -m 0755 "$ROOT/tools/orbit-wallpaper-control" "$BINDIR/orbit-wallpaper-control"

    say "Installing common shader and resource assets"
    install -m 0644 "$ROOT/wave.frag" "$APP_DATA/wave.frag"
    install -m 0644 "$ROOT/shader-blacklist.json" "$APP_DATA/shader-blacklist.json"

    if [[ ! -f "$APP_CONFIG/config" ]]; then
        say "Installing initial user configuration"
        install -m 0644 "$ROOT/orbit-wallpaper-engine.conf" "$APP_CONFIG/config"
    else
        say "Preserving existing user configuration"
    fi

    say "Installing common systemd user service"
    install -m 0644 "$ROOT/orbit-wallpaper-engine.service" "$SYSTEMD_DIR/orbit-wallpaper-engine.service"
}

install_standalone_frontend() {
    say "Installing standalone settings frontend"
    install -d "$APP_DATA/settings"
    install -m 0644 "$ROOT/settings/WallpaperSettings.qml" "$APP_DATA/settings/WallpaperSettings.qml"
    install -m 0644 "$ROOT/settings/StandaloneSettings.qml" "$APP_DATA/settings/StandaloneSettings.qml"
    install -m 0755 "$ROOT/tools/orbit-wallpaper-settings" "$BINDIR/orbit-wallpaper-settings"
    install -m 0644 "$ROOT/desktop/orbit-wallpaper-engine-settings.desktop" \
        "$APPLICATIONS_DIR/orbit-wallpaper-engine-settings.desktop"

    if ! have quickshell && [[ ! -x "$HOME/.local/opt/quickshell/bin/quickshell" ]]; then
        cat <<'EOF'

NOTE: Quickshell was not found.
The renderer is fully installed and can be configured by editing:
  ~/.config/orbit-wallpaper-engine/config

The graphical settings application will become available once Quickshell is installed.
See:
  https://quickshell.outfoxxed.me/docs/guide/install-setup/
EOF
    fi
}

install_noctalia_integration() {
    say "Installing Noctalia launcher button"
    install -d "$NOCTALIA_PLUGIN_DIR"
    for file in plugin.toml widget.luau; do
        install -m 0644 "$ROOT/integrations/noctalia/$file" "$NOCTALIA_PLUGIN_DIR/$file"
    done
}

retire_noctalia_frontend() {
    rm -rf "$NOCTALIA_PLUGIN_DIR"
}

install_core
retire_noctalia_frontend

if (( ENABLE_STEAM )); then
    say "Installing Steam game hook"
    install -m 0755 "$ROOT/scripts/orbit-wallpaper-steam-watch" \
        "$LIBEXECDIR/orbit-wallpaper-steam-watch"
    install -m 0644 "$ROOT/systemd/orbit-wallpaper-steam-watch.service" \
        "$SYSTEMD_DIR/orbit-wallpaper-steam-watch.service"
fi

if (( ENABLE_SETTINGS )); then
    install_standalone_frontend
fi

if (( NOCTALIA_INTEGRATION )); then
    install_noctalia_integration
fi

say "Reloading systemd user units"
systemctl --user daemon-reload

if (( ! NO_START )); then
    say "Enabling and starting Orbit Wallpaper Engine"
    systemctl --user enable --now orbit-wallpaper-engine.service

    if (( ENABLE_STEAM )); then
        systemctl --user enable --now orbit-wallpaper-steam-watch.service
    fi

    sleep 0.5
    if ! systemctl --user is-active --quiet orbit-wallpaper-engine.service; then
        journalctl --user -u orbit-wallpaper-engine.service -n 40 --no-pager >&2 || true
        echo "Orbit Wallpaper Engine failed to remain active." >&2
        exit 1
    fi
fi

cat <<EOF

Orbit Wallpaper Engine installed successfully.

Renderer:
  $BINDIR/orbit-wallpaper-engine

Config:
  $APP_CONFIG/config

Shaders:
  $APP_CONFIG/shaders/

Settings:
  $([[ $ENABLE_SETTINGS == 1 ]] && echo "orbit-wallpaper-settings" || echo "not installed (headless mode)")

Noctalia plugin:
  $([[ $NOCTALIA_INTEGRATION == 1 ]] && echo "$NOCTALIA_PLUGIN_DIR (launcher button)" || echo "not installed")

Steam hooks:
  $([[ $ENABLE_STEAM == 1 ]] && echo enabled || echo not installed)

The installer preserves existing config and user shaders on upgrades.
EOF
