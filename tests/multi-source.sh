#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/multi-source"
rm -rf "$TMP"
mkdir -p "$TMP/project"

cat > "$TMP/project/01_math.swan" <<'SWAN'
export C func swan_answer() -> int:
    return 42
SWAN

cat > "$TMP/project/02_main.swan" <<SWAN
import C "$TMP/project/c_api.h"
func main() -> int:
    return swan_answer() + c_answer(42) - 126
SWAN

cat > "$TMP/project/c_api.h" <<'C'
#include <stdint.h>
int64_t c_answer(int64_t value);
C
cat > "$TMP/project/native.c" <<'C'
#include <stdint.h>
int64_t swan_answer(void);
int64_t c_answer(int64_t value) { return value + swan_answer(); }
C

"$ROOT/build/swanc" build "$TMP/project" -o "$TMP/project-app"
"$TMP/project-app"

"$ROOT/build/swanc" build "$TMP/project" --emit-asm -o "$TMP/project.asm"
grep -q 'swan_answer' "$TMP/project.asm"
grep -q 'c_answer' "$TMP/project.asm"
grep -q 'main' "$TMP/project.asm"

cat > "$TMP/bad.swan" <<'SWAN'
func main() -> int:
    return (
SWAN
if "$ROOT/build/swanc" build "$TMP/bad.swan" -o "$TMP/invalid" 2>"$TMP/diagnostic"; then
    echo "expected malformed source to fail" >&2
    exit 1
fi
grep -F "$TMP/bad.swan:" "$TMP/diagnostic" >/dev/null

printf 'PASS: independent per-file lexing/parsing with project-wide semantic analysis and C linking\n'
