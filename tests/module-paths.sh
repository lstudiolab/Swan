#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swanc"
TMP="$ROOT/build/test/module-paths"
rm -rf "$TMP"
mkdir -p "$TMP/first" "$TMP/second"

cat > "$TMP/first/app.swan" <<'SWAN'
import helper

func library_answer() -> int:
    return helper_answer() + 1
SWAN

cat > "$TMP/second/helper.swan" <<'SWAN'
func helper_answer() -> int:
    return 41
SWAN

cat > "$TMP/main.swan" <<'SWAN'
import app

func main() -> int:
    !printin >> library_answer()
    return 0
SWAN

"$SWAN" build "$TMP/main.swan" -I "$TMP/first" --module-path "$TMP/second" -o "$TMP/program"
"$TMP/program" > "$TMP/actual.txt"
printf '42\n' > "$TMP/expected.txt"
diff -u "$TMP/expected.txt" "$TMP/actual.txt"

if "$SWAN" build "$TMP/main.swan" --module-path "$TMP/first" -o "$TMP/missing" 2>"$TMP/error.txt"; then
    echo "expected unresolved transitive module to fail" >&2
    exit 1
fi
grep -q "cannot resolve module 'helper'" "$TMP/error.txt"

printf 'PASS: multiple module roots and transitive source imports\n'
