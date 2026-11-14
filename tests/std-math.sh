#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swanc"
TMP="$ROOT/build/test/std-math"
mkdir -p "$TMP"

cat > "$TMP/program.swan" <<'SWAN'
import std_math

func main() -> int:
    std:print >> std:gcd(84, 30)
    std:print >> std:lcm(21, 6)
    std:print >> std:euclidean_mod(-5, 3)
    std:print >> std:floor_div(-7, 3)
    std:print >> std:ceil_div(7, 3)
    std:print >> std:integer_sqrt(144)
    std:print >> std:is_prime(97)
    std:print >> std:is_prime(99)
    std:print >> std:is_composite(99)
    std:print >> std:triangular(10)
    std:print >> std:distance_int(-4, 9)
    std:print >> std:is_power_of_two(1024)
    std:print >> std:is_power_of_two(1000)
    std:print >> std:next_power_of_two(1000)
    std:print >> std:fibonacci(10)
    return 0
SWAN

"$SWAN" build "$TMP/program.swan" -o "$TMP/program"
cat > "$TMP/expected.txt" <<'EXPECTED'
6
42
1
-3
3
12
1
0
1
55
13
1
0
1024
55
EXPECTED
"$TMP/program" > "$TMP/actual.txt"
diff -u "$TMP/expected.txt" "$TMP/actual.txt"
printf 'PASS: Swan-authored integer standard math module\n'
