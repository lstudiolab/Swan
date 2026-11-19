#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD="$ROOT/build"

test -x "$BUILD/brewc"
test -L "$BUILD/brew"
test "$(readlink "$BUILD/brew")" = "brewc"

for artifact in brew-bootstrap sil_runtime.brew sil_runtime.s sil_runtime.o; do
    if [ -e "$BUILD/$artifact" ]; then
        printf 'FAIL: obsolete compiler bootstrap artifact exists: %s\n' "$artifact" >&2
        exit 1
    fi
done

"$BUILD/brewc" --version >/dev/null
printf 'PASS: one direct C11 production compiler; no bootstrap or secondary compiler artifacts\n'
