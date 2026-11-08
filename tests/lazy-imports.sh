#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/test/lazy-imports"
mkdir -p "$TMP"
rm -rf "$TMP"/*

cat > "$TMP/unused.swan" <<'SWAN'
import foundation
fn main():
    return 0
SWAN
"$SWAN" build "$TMP/unused.swan" -o "$TMP/unused"
test ! -d "$TMP/unused.modules"
"$TMP/unused"

cat > "$TMP/used.swan" <<'SWAN'
import foundation
fn main():
    echo_int(abs_int(-9))
    return 0
SWAN
"$SWAN" build "$TMP/used.swan" -o "$TMP/used"
test -f "$TMP/used.modules/foundation.so"
nm -D --defined-only "$TMP/used.modules/foundation.so" | grep -q 'swan_fn_abs_int'
if nm -D --defined-only "$TMP/used.modules/foundation.so" | grep -q 'swan_fn_cube_int'; then
    echo "unused Foundation function was not tree-shaken" >&2
    exit 1
fi
"$TMP/used" > "$TMP/used.out"
printf '9\n' > "$TMP/expected.out"
diff -u "$TMP/expected.out" "$TMP/used.out"

cat > "$TMP/branch.swan" <<'SWAN'
import foundation
fn main():
    if false:
        foundation_hello()
    return 0
SWAN
"$SWAN" build "$TMP/branch.swan" -o "$TMP/branch"
test -f "$TMP/branch.modules/foundation.so"
mv "$TMP/branch.modules/foundation.so" "$TMP/foundation.saved"
"$TMP/branch"
mv "$TMP/foundation.saved" "$TMP/branch.modules/foundation.so"

cat > "$TMP/branch-used.swan" <<'SWAN'
import foundation
fn main():
    if true:
        foundation_hello()
    return 0
SWAN
"$SWAN" build "$TMP/branch-used.swan" -o "$TMP/branch-used"
"$TMP/branch-used" > "$TMP/branch-used.out"
grep -q 'Foundation standard library ready' "$TMP/branch-used.out"

printf 'PASS: unused imports are omitted, used functions are tree-shaken, and modules load on first call\n'
