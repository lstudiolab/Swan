#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BREW="$ROOT/build/brew"
TMP="$ROOT/build/test/foundation"
mkdir -p "$TMP"

cat > "$TMP/program.brew" <<'BREW'
import foundation
import foundation
func main():
    print(abs_int(-9))
    print(average_int(4, 7))
    print(is_even(12))
    print(is_odd(9))
    print(min_int(8, 3))
    print(max_int(8, 3))
    print(clamp_int(15, 0, 10))
    print(sign_int(-20))
    print(square_int(5))
    print(cube_int(3))
    print(power_int(2, 8))
    print(factorial_int(5))
    print(sum_to_int(10))
    print(in_range_int(5, 1, 9))
    print(abs_int(-9223372036854775807 - 1))
    print(square_int(3037000499))
    print(square_int(3037000500))
    print(cube_int(2097151))
    print(cube_int(-2097152))
    print(cube_int(2097152))
    print(power_int(2, 62))
    print(power_int(2, 63))
    print(power_int(-1, 63))
    print(factorial_int(20))
    print(factorial_int(21))
    print(sum_to_int(4294967295))
    print(sum_to_int(4294967296))
    print(average_int(9223372036854775807, 9223372036854775807))
    print(average_int(-9223372036854775807 - 1, -9223372036854775807 - 1))
    print(average_int(-9223372036854775807 - 1, 9223372036854775807))
    print(power_int(-2, 63))
    print(power_int(-2, 64))
    print(power_int(1, 9223372036854775807))
    print(power_int(-1, 9223372036854775807))
    print(power_int(0, 9223372036854775807))
    return 0
BREW

"$BREW" build "$TMP/program.brew" -o "$TMP/program"
cat > "$TMP/expected.txt" <<'EXPECTED'
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
-9223372036854775808
0
1
-1
0
EXPECTED
"$TMP/program" > "$TMP/actual.txt"
diff -u "$TMP/expected.txt" "$TMP/actual.txt"
printf 'PASS: imported Foundation helpers and print() output\n'
