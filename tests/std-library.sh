#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swanc"
TMP="$ROOT/build/test/std-library"
mkdir -p "$TMP"

cat > "$TMP/program.swan" <<'SWAN'
import std

func main() -> int:
    print("Swan std library ready")
    print(abs(-9))
    print(average(4, 7))
    print(is_even(12))
    print(is_odd(9))
    print(min(8, 3))
    print(max(8, 3))
    print(clamp(15, 0, 10))
    print(sign(-20))
    print(square(5))
    print(cube(3))
    print(power(2, 8))
    print(factorial(5))
    print(sum_to(10))
    print(in_range(5, 1, 9))
    print(abs(-9223372036854775807 - 1))
    print(square(3037000499))
    print(square(3037000500))
    print(cube(2097151))
    print(cube(-2097152))
    print(cube(2097152))
    print(power(2, 62))
    print(power(2, 63))
    print(power(-1, 63))
    print(factorial(20))
    print(factorial(21))
    print(sum_to(4294967295))
    print(sum_to(4294967296))
    print(average(9223372036854775807, 9223372036854775807))
    print(average(-9223372036854775807 - 1, -9223372036854775807 - 1))
    print(average(-9223372036854775807 - 1, 9223372036854775807))
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
0
9223372030926249001
0
9223358842721533951
-9223372036854775808
0
4611686018427387904
0
-1
2432902008176640000
0
9223372034707292160
0
9223372036854775807
-9223372036854775808
0
EXPECTED
"$TMP/program" > "$TMP/actual.txt"
diff -u "$TMP/expected.txt" "$TMP/actual.txt"
printf 'PASS: std import, print(), and standard functions\n'
