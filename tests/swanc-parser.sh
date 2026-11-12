#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p "$ROOT/build"
SOURCE="$ROOT/build/swanc-parser-module.swan"
ASSEMBLY="$ROOT/build/swanc-parser-module.s"
{
    cat "$ROOT/swanc/Parse/Lexer.swan"
    printf '\n'
    cat "$ROOT/swanc/Parse/Parser.swan"
    printf '\n'
} > "$SOURCE"
cat >> "$SOURCE" <<'SWAN'
func swanc_parser_test(source: u32_array, source_length: int, tokens: u32_array, nodes: u32_array, state: u32_array) -> int:
    let diagnostic = array_unchecked(4)
    let token_count = swan_lex(source, source_length, tokens, 128, diagnostic)
    if token_count < 1:
        return -100
    return swanc_parse(source, tokens, token_count, nodes, 128, state)
out
func swanc_lex_test(source: u32_array, source_length: int, tokens: u32_array, diagnostic: u32_array) -> int:
    return swan_lex(source, source_length, tokens, 4, diagnostic)
out
func main() -> int:
    let source = array_unchecked(1)
    let tokens = array_unchecked(5)
    let nodes = array_unchecked(5)
    let state = array_unchecked(4)
    return swanc_parser_test(source, 0, tokens, nodes, state)
out
SWAN
"$ROOT/build/swan" "$SOURCE" --target x86_64 --emit-asm -o "$ASSEMBLY"
sed -i -E 's/(^|[^[:alnum:]_])main([^[:alnum:]_]|$)/\1swan_parser_module_stub\2/g' "$ASSEMBLY"
cc -O2 "$ASSEMBLY" "$ROOT/tests/swanc_parser.c" -o "$ROOT/build/swanc-parser-test"
"$ROOT/build/swanc-parser-test"
