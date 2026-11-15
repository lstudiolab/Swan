#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swanc"
TMP="$ROOT/build/test/module-namespaces"
rm -rf "$TMP"
mkdir -p "$TMP/modules"

cat > "$TMP/modules/alpha.swan" <<'SWAN'
func value() -> int:
    return 19
SWAN
cat > "$TMP/modules/beta.swan" <<'SWAN'
func value() -> int:
    return 23
SWAN
cat > "$TMP/qualified.swan" <<'SWAN'
import alpha
import beta
func main() -> int:
    return alpha.value() + beta.value() - 42
SWAN
"$SWAN" build "$TMP/qualified.swan" -I "$TMP/modules" -o "$TMP/qualified"
"$TMP/qualified"
printf 'PASS: same-named exports resolve through module-qualified calls\n'

cat > "$TMP/ambiguous.swan" <<'SWAN'
import alpha
import beta
func main() -> int:
    return value()
SWAN
if "$SWAN" build "$TMP/ambiguous.swan" -I "$TMP/modules" -o "$TMP/ambiguous" 2>"$TMP/ambiguous.err"; then
    echo 'FAIL: ambiguous unqualified imported function was accepted' >&2
    exit 1
fi
grep -q "is ambiguous; qualify it with its module name" "$TMP/ambiguous.err"
printf 'PASS: ambiguous unqualified imports produce an actionable diagnostic\n'
