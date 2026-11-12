#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p "$ROOT/build"
SOURCE="$ROOT/build/swanc-keywords-module.swan"
ASSEMBLY="$ROOT/build/swanc-keywords-module.s"
cat "$ROOT/swanc/Parse/Lexer.swan" > "$SOURCE"
cat >> "$SOURCE" <<'SWAN'
func main() -> int:
    let word = array_unchecked(2)
    word[0] = 105
    word[1] = 102
    return swanc_keyword(word, 0, 2)
SWAN
echo "Building Compiler 2 keyword verifier..."
"$ROOT/build/swan" "$SOURCE" --target x86_64 --emit-asm -o "$ASSEMBLY"
sed -i -E 's/(^|[^[:alnum:]_])main([^[:alnum:]_]|$)/\1swan_keywords_module_stub\2/g' "$ASSEMBLY"
echo "Linking keyword regression harness..."
cc -O2 "$ASSEMBLY" "$ROOT/tests/swanc_keywords.c" -o "$ROOT/build/swanc-keywords-test"
echo "Running keyword regression harness..."
"$ROOT/build/swanc-keywords-test"
