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
    let source = array_unchecked(1)
    let tokens = array_unchecked(5)
    let diagnostic = array_unchecked(4)
    return swan_lex(source, 0, tokens, 1, diagnostic)
SWAN
"$ROOT/build/swan" "$SOURCE" --target x86_64 --emit-asm -o "$ASSEMBLY"
sed -i -E 's/(^|[^[:alnum:]_])main([^[:alnum:]_]|$)/\1swan_keywords_module_stub\2/g' "$ASSEMBLY"
cc -O2 "$ASSEMBLY" "$ROOT/tests/swanc_keywords.c" -o "$ROOT/build/swanc-keywords-test"
"$ROOT/build/swanc-keywords-test"
