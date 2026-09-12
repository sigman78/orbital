#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
case "$(uname -s)-$(uname -m)" in
  Linux-x86_64)
    package=linux-x86_64-glibc-2.27
    checksum=427d9985aac9e88912429bf9e7c1a3543bf35a61aaa855c890eeb96920175e0c ;;
  Darwin-arm64)
    package=macos-aarch64
    checksum=92da7ab6226dd951037cd85397f830ae78fe40fbbb8928882e0b2654e468fdd4 ;;
  Darwin-x86_64)
    package=macos-x86_64
    checksum=adc5e179f5584ca572293e93612df9f0b6be8a46dcb53622238efc7a62b1da2f ;;
  *) echo 'Supported hosts: Linux x86_64 and macOS arm64/x86_64.' >&2; exit 1 ;;
esac
mkdir -p "$root/.tools"
archive="$(mktemp "$root/.tools/slang.XXXXXX")"
trap 'rm -f -- "$archive"' EXIT
curl -fL --retry 3 "https://github.com/shader-slang/slang/releases/download/v2026.14.1/slang-2026.14.1-${package}.tar.gz" -o "$archive"
if [[ "$(uname -s)" == Darwin ]]; then
  echo "$checksum  $archive" | shasum -a 256 --check
else
  echo "$checksum  $archive" | sha256sum --check
fi
mkdir -p "$root/.tools/slang"
tar -xzf "$archive" -C "$root/.tools/slang"
"$root/.tools/slang/bin/slangc" -version
