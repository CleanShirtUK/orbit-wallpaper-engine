#!/usr/bin/env bash
set -Eeuo pipefail

APP="orbit-wallpaper-engine"
PURGE=0

if [[ "${1:-}" == "--purge" ]]; then
    PURGE=1
elif [[ -n "${1:-}" ]]; then
    echo "Usage: ./uninstall.sh [--purge]" >&2
    exit 2
fi

PREFIX="${PREFIX:-$HOME/.local}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
CACHE_HOME="${XDG_CACHE_HOME:-$HOME/.cache}"
NOCTALIA_PLUGIN_DIR="$DATA_HOME/noctalia/plugins/orbit-wallpaper-engine"

systemctl --user disable --now orbit-wallpaper-steam-watch.service 2>/dev/null || true
systemctl --user disable --now orbit-wallpaper-engine.service 2>/dev/null || true

rm -f \
    "$PREFIX/bin/orbit-wallpaper-engine" \
    "$PREFIX/bin/orbit-wallpaper-helper" \
    "$PREFIX/bin/orbit-wallpaper-settings" \
    "$CONFIG_HOME/systemd/user/orbit-wallpaper-engine.service" \
    "$CONFIG_HOME/systemd/user/orbit-wallpaper-steam-watch.service" \
    "$DATA_HOME/applications/orbit-wallpaper-engine-settings.desktop" \
    "$PREFIX/bin/orbit-wallpaper-control"

rm -rf \
    "$PREFIX/libexec/orbit-wallpaper-engine" \
    "$DATA_HOME/orbit-wallpaper-engine" \
    "$NOCTALIA_PLUGIN_DIR"

systemctl --user daemon-reload

if (( PURGE )); then
    rm -rf \
        "$CONFIG_HOME/orbit-wallpaper-engine" \
        "$CACHE_HOME/orbit-wallpaper-engine"
    echo "Orbit Wallpaper Engine and user data removed."
else
    echo "Orbit Wallpaper Engine removed."
    echo "User config and shaders were preserved in:"
    echo "  $CONFIG_HOME/orbit-wallpaper-engine"
    echo "Run ./uninstall.sh --purge to remove them too."
fi
