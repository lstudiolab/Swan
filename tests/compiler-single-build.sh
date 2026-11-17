#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD="$ROOT/build"

test -x "$BUILD/swanc"
test -L "$BUILD/swan"
test "$(readlink "$BUILD/swan")" = "swanc"

for artifact in swan-bootstrap sil_runtime.swan sil_runtime.s sil_runtime.o; do
    if [ -e "$BUILD/$artifact" ]; then
        printf 'FAIL: obsolete compiler bootstrap artifact exists: %s\n' "$artifact" >&2
        exit 1
    fi
done

"$BUILD/swanc" --version >/dev/null
printf 'PASS: one direct C11 production compiler; no bootstrap or secondary compiler artifacts\n'
