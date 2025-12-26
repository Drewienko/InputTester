#!/usr/bin/env bash
set -euo pipefail

buildType="${1:-Release}"
qtPrefixPath="${QT_PREFIX_PATH:-$HOME/Qt/6.10.1/gcc_64}"

buildDir="build-linux-$(echo "$buildType" | tr '[:upper:]' '[:lower:]')"

cmake -S . -B "$buildDir" -G Ninja -DCMAKE_BUILD_TYPE="$buildType" -DCMAKE_PREFIX_PATH="$qtPrefixPath"
cmake --build "$buildDir"
