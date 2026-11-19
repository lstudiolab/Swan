#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BREW="$ROOT/build/brewc"
TMP="$ROOT/build/test/module-namespaces"
rm -rf "$TMP"
mkdir -p "$TMP/modules"

cat > "$TMP/modules/alpha.brew" <<'BREW'
func value() -> int:
    return 19
BREW
cat > "$TMP/modules/beta.brew" <<'BREW'
func value() -> int:
    return 23
BREW
cat > "$TMP/qualified.brew" <<'BREW'
import alpha
import beta
func main() -> int:
    return alpha.value() + beta.value() - 42
BREW
"$BREW" build "$TMP/qualified.brew" -I "$TMP/modules" -o "$TMP/qualified"
"$TMP/qualified"
printf 'PASS: same-named exports resolve through module-qualified calls\n'

cat > "$TMP/ambiguous.brew" <<'BREW'
import alpha
import beta
func main() -> int:
    return value()
BREW
if "$BREW" build "$TMP/ambiguous.brew" -I "$TMP/modules" -o "$TMP/ambiguous" 2>"$TMP/ambiguous.err"; then
    echo 'FAIL: ambiguous unqualified imported function was accepted' >&2
    exit 1
fi
grep -q "is ambiguous; qualify it with its module name" "$TMP/ambiguous.err"
printf 'PASS: ambiguous unqualified imports produce an actionable diagnostic\n'
