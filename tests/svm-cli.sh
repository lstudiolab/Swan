#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
printf 'fn main() -> int:\n    return 42\n' > "$TMP/hello.swan"
"$BIN" build "$TMP/hello.swan" --target svm -o "$TMP/hello.swvm"
test -s "$TMP/hello.swvm"
set +e
"$BIN" run "$TMP/hello.swan" --target svm
STATUS=$?
set -e
test "$STATUS" -eq 42
printf 'PASS: Swan CLI emits and executes portable SVM modules\n'
