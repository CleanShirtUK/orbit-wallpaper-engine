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

INSTALL_DEPS=0
ENABLE_STEAM=1
ENABLE_SETTINGS=1
NO_START=0
CHECK_ONLY=0

usage() {
    cat <<EOF
Usage: ./install.sh [options]

Options:
  --install-deps     Offer to install missing build dependencies.
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
        --no-steam-hooks) ENABLE_STEAM=0 ;;
        --no-settings) ENABLE_SETTINGS=0 ;;
        --no-start) NO_START=1 ;;
        --check) CHECK_ONLY=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

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

say "Build dependencies are available"

if (( CHECK_ONLY )); then
    if have quickshell || [[ -x "$HOME/.local/opt/quickshell/bin/quickshell" ]]; then
        echo "Quickshell: available"
    else
        echo "Quickshell: not found (optional; graphical settings UI only)"
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

say "Creating user installation directories"
install -d \
    "$BINDIR" \
    "$LIBEXECDIR" \
    "$APP_DATA/settings" \
    "$APP_CONFIG/shaders" \
    "$CACHE_HOME/$APP" \
    "$SYSTEMD_DIR" \
    "$APPLICATIONS_DIR"

say "Installing renderer and backend"
install -m 0755 "$ROOT/$APP" "$BINDIR/$APP"
install -m 0755 "$ROOT/tools/orbit-wallpaper-helper" "$BINDIR/orbit-wallpaper-helper"

say "Installing default shader and settings assets"
install -m 0644 "$ROOT/wave.frag" "$APP_DATA/wave.frag"
install -m 0644 "$ROOT/shader-blacklist.json" "$APP_DATA/shader-blacklist.json"
install -m 0644 "$ROOT/settings/WallpaperSettings.qml" "$APP_DATA/settings/WallpaperSettings.qml"
install -m 0644 "$ROOT/settings/StandaloneSettings.qml" "$APP_DATA/settings/StandaloneSettings.qml"

if [[ ! -f "$APP_CONFIG/config" ]]; then
    say "Installing initial user configuration"
    install -m 0644 "$ROOT/orbit-wallpaper-engine.conf" "$APP_CONFIG/config"
else
    say "Preserving existing user configuration"
fi

say "Installing systemd user service"
install -m 0644 "$ROOT/orbit-wallpaper-engine.service" "$SYSTEMD_DIR/orbit-wallpaper-engine.service"

if (( ENABLE_STEAM )); then
    say "Installing Steam game hook"
    install -m 0755 "$ROOT/scripts/orbit-wallpaper-steam-watch" \
        "$LIBEXECDIR/orbit-wallpaper-steam-watch"
    install -m 0644 "$ROOT/systemd/orbit-wallpaper-steam-watch.service" \
        "$SYSTEMD_DIR/orbit-wallpaper-steam-watch.service"
fi

if (( ENABLE_SETTINGS )); then
    say "Installing graphical settings launcher"
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
  orbit-wallpaper-settings

Steam hooks:
  $([[ $ENABLE_STEAM == 1 ]] && echo enabled || echo not installed)

The installer preserves existing config and user shaders on upgrades.
EOF
