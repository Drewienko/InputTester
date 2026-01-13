#!/usr/bin/env bash
set -euo pipefail

config="${1:-Release}"
config_lower="$(echo "$config" | tr '[:upper:]' '[:lower:]')"

case "$config_lower" in
  debug) preset="linux-debug-gcc" ;;
  release) preset="linux-release-gcc" ;;
  *)
    echo "Usage: $0 [Debug|Release]" >&2
    exit 1
    ;;
esac

qt_prefix="${QT6_PREFIX:-$HOME/Qt/6.10.1/gcc_64}"
export QT6_PREFIX="$qt_prefix"

cmake --preset "$preset"
cmake --build --preset "$preset"
