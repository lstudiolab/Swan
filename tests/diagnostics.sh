#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swanc"
TMP="$ROOT/build/diagnostics"
mkdir -p "$TMP"
rm -f "$TMP/warnings-as-errors.s" "$TMP/error.s"

cat > "$TMP/warnings.swan" <<'SWAN'
func unused_helper() -> int:
    return 7

func main() -> int:
    var value = 1
    value = value
    log 4 / 0
    if true:
        log value
    return 0
SWAN

"$SWAN" "$TMP/warnings.swan" --emit-asm -o "$TMP/warnings.s" 2>"$TMP/warnings.err"
grep -q "self-assignment to 'value' has no effect" "$TMP/warnings.err"
grep -q "condition is always true" "$TMP/warnings.err"
grep -q "literal zero" "$TMP/warnings.err"
grep -q "function 'unused_helper' is never reached from main" "$TMP/warnings.err"
printf 'PASS: semantic warnings identify suspicious constructs without stopping compilation\n'

if "$SWAN" "$TMP/warnings.swan" --emit-asm --warnings-as-errors -o "$TMP/warnings-as-errors.s" 2>"$TMP/warnings-as-errors.err"; then
    echo 'FAIL: --warnings-as-errors allowed compilation to continue' >&2
    exit 1
fi
grep -q "compilation stopped" "$TMP/warnings-as-errors.err"
test ! -e "$TMP/warnings-as-errors.s"
printf 'PASS: --warnings-as-errors stops before code generation\n'

cat > "$TMP/error.swan" <<'SWAN'
func main() -> int:
    log missing_symbol
    return 0
SWAN
if "$SWAN" "$TMP/error.swan" --emit-asm -o "$TMP/error.s" 2>"$TMP/error.err"; then
    echo 'FAIL: semantic error was accepted' >&2
    exit 1
fi
test ! -e "$TMP/error.s"
printf 'PASS: semantic errors stop compilation before output generation\n'
