#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
executable="$(realpath "${1:-$root/build/linux-debug/orbital}")"
output="$root/captures/linux-smoke"
mkdir -p "$output"

# An SSH shell can use this user's existing graphical login without hardcoded display/auth paths.
if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]] && command -v systemctl >/dev/null; then
    while IFS= read -r entry; do
        case "$entry" in DISPLAY=*|WAYLAND_DISPLAY=*|XDG_RUNTIME_DIR=*|XAUTHORITY=*) export "$entry" ;; esac
    done < <(systemctl --user show-environment)
fi

run() {
    local name="$1"
    shift
    timeout 180s "$executable" --width 960 --height 540 --frames 40 --time 0 \
        --capture "$output/$name.png" "$@" >"$output/$name.log" 2>&1
    if grep -E 'NoGraphicsAPI validation:|Validation Error|error:' "$output/$name.log"; then
        echo "Rendering/validation failed: $name" >&2
        exit 1
    fi
    grep -q 'Completed 40 frames.' "$output/$name.log"
    test -s "$output/$name.png"
    echo "Passed: $name"
}

run earth --no-hud
run earth-repeat --no-hud
if ! cmp -s "$output/earth.png" "$output/earth-repeat.png"; then
    echo 'Fixed-time captures differ; retain both for pixel comparison.'
fi
run belt --bookmark 5 --high --no-hud --benchmark "$output/belt.csv"
run ui --ui
run maximize --maximize-at 8 --no-hud
run fullscreen --fullscreen-at 8 --no-hud
echo "Linux rendering smoke passed; captures and logs: $output"
