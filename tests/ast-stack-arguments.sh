#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BREW="$ROOT/build/brewc"
TMP="$ROOT/build/test/ast-stack-arguments"
mkdir -p "$TMP"
cat > "$TMP/program.brew" <<'BREW'
func get_value() -> int:
    return 19

func sum20(values: u32_array, a1: int, a2: int, a3: int, a4: int, a5: int, a6: int, a7: int, a8: int, a9: int, a10: int, a11: int, a12: int, a13: int, a14: int, a15: int, a16: int, a17: int, a18: int, a19: int) -> int:
    return values[0] + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11 + a12 + a13 + a14 + a15 + a16 + a17 + a18 + a19

func main() -> int:
    var values = array_u32(1)
    values[0] = 42
    return sum20(values, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, get_value()) - 232
BREW
"$BREW" build "$TMP/program.brew" -o "$TMP/program"
"$TMP/program"
printf 'PASS: AST backend stack arguments and nested argument staging beyond six parameters\n'
