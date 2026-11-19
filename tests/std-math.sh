#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BREW="$ROOT/build/brewc"
TMP="$ROOT/build/test/std-math"
mkdir -p "$TMP"

cat > "$TMP/program.brew" <<'BREW'
import std_math

func main() -> int:
    print(gcd(84, 30))
    print(lcm(21, 6))
    print(euclidean_mod(-5, 3))
    print(floor_div(-7, 3))
    print(ceil_div(7, 3))
    print(integer_sqrt(144))
    print(is_prime(97))
    print(is_prime(99))
    print(is_composite(99))
    print(triangular(10))
    print(distance_int(-4, 9))
    print(is_power_of_two(1024))
    print(is_power_of_two(1000))
    print(next_power_of_two(1000))
    print(fibonacci(10))
    print(integer_sqrt(9223372036854775807))
    print(next_power_of_two(4611686018427387904))
    print(next_power_of_two(4611686018427387905))
    print(gcd(-84, -30))
    print(gcd(-9223372036854775807 - 1, 1))
    print(gcd(-9223372036854775807 - 1, 0))
    print(lcm(-9223372036854775807 - 1, 1))
    print(floor_div(-9223372036854775807 - 1, 3))
    print(ceil_div(9223372036854775807, 3))
    print(floor_div(-9223372036854775807 - 1, -1))
    print(triangular(4294967295))
    print(triangular(9223372036854775807))
    print(distance_int(-9223372036854775807 - 1, 9223372036854775807))
    print(fibonacci(92))
    print(fibonacci(93))
    return 0
BREW

"$BREW" build "$TMP/program.brew" -o "$TMP/program"
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
3037000499
4611686018427387904
0
6
1
0
0
-3074457345618258603
3074457345618258603
0
9223372034707292160
0
0
7540113804746346429
0
EXPECTED
"$TMP/program" > "$TMP/actual.txt"
diff -u "$TMP/expected.txt" "$TMP/actual.txt"
printf 'PASS: Brew-authored integer standard math module\n'
