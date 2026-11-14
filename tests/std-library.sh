#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swanc"
TMP="$ROOT/build/test/std-library"
mkdir -p "$TMP"

cat > "$TMP/program.swan" <<'SWAN'
import std

func main() -> int:
    std:print >> "Swan std library ready"
    std:print >> std:abs(-9)
    std:print >> std:average(4, 7)
    std:print >> std:is_even(12)
    std:print >> std:is_odd(9)
    std:print >> std:min(8, 3)
    std:print >> std:max(8, 3)
    std:print >> std:clamp(15, 0, 10)
    std:print >> std:sign(-20)
    std:print >> std:square(5)
    std:print >> std:cube(3)
    std:print >> std:power(2, 8)
    std:print >> std:factorial(5)
    std:print >> std:sum_to(10)
    std:print >> std:in_range(5, 1, 9)
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
printf 'PASS: std namespace, std:print, and standard functions\n'
