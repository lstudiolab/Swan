#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swanc"
TMP="$ROOT/build/test/std-library"
mkdir -p "$TMP"

cat > "$TMP/program.swan" <<'SWAN'
import std

func main() -> int:
    !printin >> "Swan std library ready"
    !printin >> !abs(-9)
    !printin >> !average(4, 7)
    !printin >> !is_even(12)
    !printin >> !is_odd(9)
    !printin >> !min(8, 3)
    !printin >> !max(8, 3)
    !printin >> !clamp(15, 0, 10)
    !printin >> !sign(-20)
    !printin >> !square(5)
    !printin >> !cube(3)
    !printin >> !power(2, 8)
    !printin >> !factorial(5)
    !printin >> !sum_to(10)
    !printin >> !in_range(5, 1, 9)
    return 0
SWAN

"$SWAN" build "$TMP/program.swan" -o "$TMP/program"
cat > "$TMP/expected.txt" <<'EXPECTED'
Swan std library ready
9
5
1
1
3
8
10
-1
25
27
256
120
55
1
EXPECTED
"$TMP/program" > "$TMP/actual.txt"
diff -u "$TMP/expected.txt" "$TMP/actual.txt"
printf 'PASS: std namespace, !printin, and standard functions\n'
