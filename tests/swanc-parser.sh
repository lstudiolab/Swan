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
func swanc_parser_test(source: u32_array, source_length: int, tokens: u32_array, token_capacity: int, nodes: u32_array, node_capacity: int, state: u32_array) -> int:
    let diagnostic = array_unchecked(4)
    let token_count = swan_lex(source, source_length, tokens, token_capacity, diagnostic)
    if token_count < 1:
        return -100
    return swanc_parse(source, tokens, token_count, nodes, node_capacity, state)
out
SWAN
"$ROOT/build/swan" "$SOURCE" --target x86_64 --emit-asm -o "$ASSEMBLY"
sed -i -E 's/(^|[^[:alnum:]_])swanc_parser_test([^[:alnum:]_]|$)/\1swan_parser_test_stub\2/g' "$ASSEMBLY"
cc -O2 "$ASSEMBLY" "$ROOT/tests/swanc_parser.c" -o "$ROOT/build/swanc-parser-test"
"$ROOT/build/swanc-parser-test"
