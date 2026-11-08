#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p build

echo "=== Swan ==="
/usr/bin/time -f 'Swan compile: %e seconds | compiler peak RSS: %M KiB' ./build/swan benchmarks/sort_1b.swan --emit-asm -o build/sort_swan.s
/usr/bin/time -f 'Swan link: %e seconds | linker peak RSS: %M KiB' cc -x assembler build/sort_swan.s -o build/sort_swan

run_and_check() {
  local name="$1"; shift
  local output status
  echo "Starting $name 1B sort"
  if output=$(/usr/bin/time -f "$name runtime: %e seconds | user CPU: %U seconds | system CPU: %S seconds | peak RSS: %M KiB" "$@" 2> "build/${name}.time"); then
    cat "build/${name}.time"
  else
    status=$?
    cat "build/${name}.time"
    echo "$name process failed with exit code $status" >&2
    return "$status"
  fi
  if [[ "$output" != "1" ]]; then
    echo "$name verification failed: expected 1, got '$output'" >&2
    return 1
  fi
  echo "$name verification: PASS"
}
run_and_check Swan ./build/sort_swan

echo "=== Goose ==="
/usr/bin/time -f 'Goose compile + link: %e seconds | compiler peak RSS: %M KiB' ./goose-build/goose build --cc gcc --stack-reserve 8G -o build/sort_goose benchmarks/sort_1b.goose
run_and_check Goose ./build/sort_goose

echo "=== Rust ==="
/usr/bin/time -f 'Rust compile + link: %e seconds | compiler peak RSS: %M KiB' rustc -C opt-level=2 -C target-cpu=native benchmarks/sort_1b.rs -o build/sort_rust
run_and_check Rust ./build/sort_rust

echo "=== C ==="
/usr/bin/time -f 'C compile + link: %e seconds | compiler peak RSS: %M KiB' cc -std=c11 -O2 -march=native -DNDEBUG benchmarks/sort_1b.c -o build/sort_c
run_and_check C ./build/sort_c
