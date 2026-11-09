#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/utf8-tests"
mkdir -p "$TMP"

cat > "$TMP/unicode.swan" <<'SWAN'
fn main():
    let café = 40
    let 你好 = 2
    log "UTF-8: café, 你好, 日本語, 🦢"
    log café + 你好
    return 0
SWAN
"$SWAN" "$TMP/unicode.swan" --emit-asm -o "$TMP/unicode.s"
cc -x assembler "$TMP/unicode.s" -o "$TMP/unicode"
actual=$("$TMP/unicode")
expected=$(printf 'UTF-8: café, 你好, 日本語, 🦢\n42')
test "$actual" = "$expected"
printf 'PASS: UTF-8 string literals and Unicode identifiers\n'

# A UTF-8 BOM is accepted and removed before tokenization.
printf '\357\273\277fn main():\n    log "BOM accepted"\n    return 0\n' > "$TMP/bom.swan"
test "$("$SWAN" "$TMP/bom.swan")" = "BOM accepted"
printf 'PASS: UTF-8 BOM handling\n'

# Invalid byte sequences are rejected instead of being passed to later stages.
printf 'fn main():\n    log "bad: \377"\n    return 0\n' > "$TMP/invalid.swan"
if "$SWAN" "$TMP/invalid.swan" --emit-asm -o "$TMP/invalid.s" 2>"$TMP/invalid.err"; then
    echo 'FAIL: invalid UTF-8 source was accepted' >&2
    exit 1
fi
grep -q 'source is not valid UTF-8' "$TMP/invalid.err"
printf 'PASS: malformed UTF-8 rejected\n'
