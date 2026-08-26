#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

say() {
    printf '\n==> %s\n' "$*"
}

say "Checking required release files"
for f in \
    VERSION \
    LICENSE \
    README.md \
    CHANGELOG.md \
    CONTRIBUTING.md \
    SECURITY.md \
    SHADER_POLICY.md \
    AI_DISCLOSURE.md \
    THIRD_PARTY_NOTICES.md \
    shader-blacklist.json
do
    [[ -s "$f" ]] || fail "Missing or empty: $f"
done

say "Checking blacklist JSON"
python3 - <<'PY'
import json
from pathlib import Path

value = json.loads(Path("shader-blacklist.json").read_text())
assert isinstance(value, dict)
assert isinstance(value.get("blocked"), list)
for item in value["blocked"]:
    assert isinstance(item, dict)
    assert str(item.get("id", "")).strip()
PY

say "Checking shell syntax"
bash -n install.sh
bash -n uninstall.sh
bash -n tools/orbit-wallpaper-settings
bash -n examples/generic/signal-wallpaper
bash -n examples/hyprland/wallpaper-animation
bash -n examples/hyprland/animate-shutdown
bash -n examples/hyprland/animate-lock
bash -n examples/hyprland/animate-login
bash -n tools/test-integrations.sh

say "Checking Python syntax"
python3 -m py_compile \
    tools/orbit-wallpaper-helper \
    scripts/orbit-wallpaper-steam-watch
rm -rf tools/__pycache__ scripts/__pycache__

say "Checking Git whitespace"
git diff --check

say "Checking for accidentally bundled catalogue shaders"
if [[ -d shaders ]] && find shaders -type f -print -quit | grep -q .; then
    fail "Third-party shader files are present under ./shaders"
fi

say "Checking user-specific paths"
if grep -RInE \
    --exclude-dir=.git \
    --exclude=RELEASE_CHECKLIST.md \
    --binary-files=without-match \
    '/home/[A-Za-z0-9._-]+|/Users/[A-Za-z0-9._-]+' \
    renderer.c install.sh uninstall.sh settings tools scripts systemd desktop orbit-wallpaper-engine.conf
then
    fail "User-specific absolute paths remain in runtime/release files"
fi

say "Checking stale active project identity"
if grep -RInE \
    --exclude-dir=.git \
    --exclude=README.md \
    --exclude=MIGRATION.md \
    --exclude=.gitignore \
    --exclude=renderer.c \
    --exclude=release-check.sh \
    --binary-files=without-match \
    'ps3-wave-wallpaper|PS3 Wave Wallpaper|PS3-style|PS3 style' \
    Makefile install.sh uninstall.sh settings tools scripts systemd desktop orbit-wallpaper-engine.conf
then
    fail "Stale project identity found outside documented migration compatibility"
fi

say "Testing integration behaviour"
./tools/test-integrations.sh

say "Checking build dependencies"
./install.sh --check

say "Building"
make clean
make

say "Checking expected binary"
[[ -x orbit-wallpaper-engine ]] || fail "Build did not produce orbit-wallpaper-engine"

say "Cleaning local build output"
make clean

say "Release checks passed"
