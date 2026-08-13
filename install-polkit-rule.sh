#!/bin/sh
set -eu

install -o root -g root -m 0644 \
    "$(dirname "$0")/polkit/49-noctalia-greeter.rules" \
    /etc/polkit-1/rules.d/49-noctalia-greeter.rules

printf '%s\n' "Installed Noctalia greeter polkit rule. Trigger a greeter sync to verify it."
