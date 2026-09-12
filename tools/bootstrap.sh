#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
[[ "$(uname -s)-$(uname -m)" == Linux-x86_64 ]] || { echo 'This bootstrap requires Linux x86_64.' >&2; exit 1; }
mkdir -p "$root/.tools"
archive="$(mktemp "$root/.tools/slang.XXXXXX.tar.gz")"
trap 'rm -f -- "$archive"' EXIT
curl -fL --retry 3 'https://github.com/shader-slang/slang/releases/download/v2026.14.1/slang-2026.14.1-linux-x86_64-glibc-2.27.tar.gz' -o "$archive"
echo "427d9985aac9e88912429bf9e7c1a3543bf35a61aaa855c890eeb96920175e0c  $archive" | sha256sum --check
mkdir -p "$root/.tools/slang"
tar -xzf "$archive" -C "$root/.tools/slang"
"$root/.tools/slang/bin/slangc" -version
