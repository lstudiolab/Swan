#!/usr/bin/env sh
set -eu
set -x
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BREW="$ROOT/build/brew"
TMP="$ROOT/build/test/lazy-imports"
mkdir -p "$TMP"
rm -rf "$TMP"/*

cat > "$TMP/unused.brew" <<'BREW'
lazy import foundation
func main():
    return 0
BREW
"$BREW" build "$TMP/unused.brew" --lazy-imports -o "$TMP/unused"
test ! -d "$TMP/unused.modules"
"$TMP/unused"

cat > "$TMP/used.brew" <<'BREW'
import foundation
func main():
    print(abs_int(-9))
    return 0
BREW
"$BREW" build "$TMP/used.brew" --lazy-imports -o "$TMP/used"
test -f "$TMP/used.modules/foundation.so"
nm -D --defined-only "$TMP/used.modules/foundation.so" | grep -q 'brew_fn___brew_module_0_abs_int'
if nm -D --defined-only "$TMP/used.modules/foundation.so" | grep -q 'brew_fn___brew_module_0_cube_int'; then
    echo "unused Foundation function was not tree-shaken" >&2
    exit 1
fi
"$TMP/used" > "$TMP/used.out"
printf '9\n' > "$TMP/expected.out"
diff -u "$TMP/expected.out" "$TMP/used.out"

cat > "$TMP/branch.brew" <<'BREW'
import foundation
func main():
    if false:
        foundation_hello()
    return 0
BREW
"$BREW" build "$TMP/branch.brew" --lazy-imports -o "$TMP/branch"
test -f "$TMP/branch.modules/foundation.so"
mv "$TMP/branch.modules/foundation.so" "$TMP/foundation.saved"
"$TMP/branch"
mv "$TMP/foundation.saved" "$TMP/branch.modules/foundation.so"

cat > "$TMP/branch-used.brew" <<'BREW'
import foundation
func main():
    if true:
        foundation_hello()
    return 0
BREW
"$BREW" build "$TMP/branch-used.brew" --lazy-imports -o "$TMP/branch-used"
"$TMP/branch-used" > "$TMP/branch-used.out"
test ! -s "$TMP/branch-used.out"

printf 'PASS: unused imports are omitted, used functions are tree-shaken, and modules load on first call\n'
