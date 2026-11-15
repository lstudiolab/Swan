#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swanc"
TMP="$ROOT/build/test/std-math"
mkdir -p "$TMP"

cat > "$TMP/program.swan" <<'SWAN'
import std_math

func main() -> int:
    !printin >> !gcd(84, 30)
    !printin >> !lcm(21, 6)
    !printin >> !euclidean_mod(-5, 3)
    !printin >> !floor_div(-7, 3)
    !printin >> !ceil_div(7, 3)
    !printin >> !integer_sqrt(144)
    !printin >> !is_prime(97)
    !printin >> !is_prime(99)
    !printin >> !is_composite(99)
    !printin >> !triangular(10)
    !printin >> !distance_int(-4, 9)
    !printin >> !is_power_of_two(1024)
    !printin >> !is_power_of_two(1000)
    !printin >> !next_power_of_two(1000)
    !printin >> !fibonacci(10)
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
