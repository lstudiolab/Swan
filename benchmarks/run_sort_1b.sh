#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p build
/usr/bin/time -f 'Swan compiler build: %e seconds | peak RSS: %M KiB' sh build.sh
/usr/bin/time -f 'Swan source compilation: %e seconds | peak RSS: %M KiB' ./build/swan benchmarks/sort_1b.swan --emit-asm -o build/sort_1b.s
/usr/bin/time -f 'Assembly and link: %e seconds | peak RSS: %M KiB' cc -x assembler build/sort_1b.s -o build/sort_1b
echo 'Starting 1,000,000,000-element sort; requires approximately 4 GB for the array.'
result=$(/usr/bin/time -f 'Sort + verification: %e seconds | user CPU: %U seconds | system CPU: %S seconds | peak RSS: %M KiB' ./build/sort_1b)
if [ "$result" != "1" ]; then
    echo "Sort verification failed: expected 1, got $result" >&2
    exit 1
fi
echo 'Sorted-order verification: PASS'
