#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CXX=${CXX:-c++}
mkdir -p "$ROOT/build"
SOURCES=$(find "$ROOT/compiler/src" -type f -name '*.cpp' | sort)
# shellcheck disable=SC2086
"$CXX" -std=c++20 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -I"$ROOT/compiler/include" $SOURCES -o "$ROOT/build/swan"
printf 'Built %s\\n' "$ROOT/build/swan"
