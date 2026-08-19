#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
GENERIC="$ROOT/examples/generic/signal-wallpaper"
HYPR="$ROOT/examples/hyprland/wallpaper-animation"
SHUTDOWN="$ROOT/examples/hyprland/animate-shutdown"

fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
pass() { printf 'PASS: %s\n' "$*"; }

[[ -x "$GENERIC" ]] || fail "Missing executable: $GENERIC"
[[ -x "$HYPR" ]] || fail "Missing executable: $HYPR"
[[ -x "$SHUTDOWN" ]] || fail "Missing executable: $SHUTDOWN"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

CACHE="$TMP/cache"
FIFO_DIR="$CACHE/orbit-wallpaper-engine"
FIFO="$FIFO_DIR/control"
mkdir -p "$FIFO_DIR"

run_fast() {
    local name="$1"
    shift
    local start end elapsed
    start="$(date +%s%N)"
    "$@"
    end="$(date +%s%N)"
    elapsed=$(( (end - start) / 1000000 ))
    (( elapsed < 2000 )) || fail "$name took ${elapsed}ms"
    printf '%s\n' "$elapsed"
}

echo "==> Missing FIFO"
rm -f "$FIFO"
elapsed="$(run_fast "generic missing FIFO" env XDG_CACHE_HOME="$CACHE" "$GENERIC" exit)"
pass "generic missing FIFO (${elapsed}ms)"
elapsed="$(run_fast "Hyprland missing FIFO" env XDG_CACHE_HOME="$CACHE" "$HYPR" exit)"
pass "Hyprland missing FIFO (${elapsed}ms)"

echo
echo "==> FIFO with no reader"
rm -f "$FIFO"
mkfifo "$FIFO"
elapsed="$(run_fast "generic dead FIFO" env XDG_CACHE_HOME="$CACHE" "$GENERIC" exit)"
pass "generic dead FIFO bounded (${elapsed}ms)"
elapsed="$(run_fast "Hyprland dead FIFO" env XDG_CACHE_HOME="$CACHE" "$HYPR" exit)"
pass "Hyprland dead FIFO bounded (${elapsed}ms)"

echo
echo "==> Live FIFO command delivery"

test_delivery() {
    local script="$1"
    local command="$2"
    local name="$3"
    local capture="$TMP/capture-$RANDOM"
    rm -f "$FIFO"
    mkfifo "$FIFO"
    cat "$FIFO" > "$capture" &
    local reader=$!
    env XDG_CACHE_HOME="$CACHE" "$script" "$command"
    wait "$reader"
    local received
    received="$(cat "$capture")"
    [[ "$received" == "$command" ]] || fail "$name delivered '$received', expected '$command'"
    pass "$name delivered '$command'"
}

for command in intro exit palette; do
    test_delivery "$GENERIC" "$command" "generic helper"
    test_delivery "$HYPR" "$command" "Hyprland helper"
done

echo
echo "==> Hyprland logout flow with mocked loginctl"

MOCKBIN="$TMP/mockbin"
mkdir -p "$MOCKBIN"
LOGINCTL_LOG="$TMP/loginctl.log"

cat > "$MOCKBIN/loginctl" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" > "${ORBIT_TEST_LOGINCTL_LOG:?}"
exit 0
EOF
chmod +x "$MOCKBIN/loginctl"

rm -f "$FIFO"
mkfifo "$FIFO"

start="$(date +%s%N)"
env \
  PATH="$MOCKBIN:$PATH" \
  XDG_CACHE_HOME="$CACHE" \
  XDG_SESSION_ID="orbit-test-session" \
  ORBIT_TEST_LOGINCTL_LOG="$LOGINCTL_LOG" \
  "$SHUTDOWN"
end="$(date +%s%N)"
elapsed=$(( (end - start) / 1000000 ))

(( elapsed < 3000 )) || fail "animate-shutdown took ${elapsed}ms; wallpaper likely blocked logout"
[[ -f "$LOGINCTL_LOG" ]] || fail "mock loginctl was never called"
called="$(cat "$LOGINCTL_LOG")"
[[ "$called" == "terminate-session orbit-test-session" ]] || fail "unexpected loginctl call: $called"

pass "logout proceeds after unavailable renderer (${elapsed}ms)"
pass "mock loginctl received correct session"

echo
echo "All integration behaviour tests passed."
