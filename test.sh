#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"
sh "$ROOT/build.sh"
sh "$ROOT/tests/compiler-single-build.sh"
sh "$ROOT/tests/headers.sh"
cc -std=c11 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" "$ROOT/tests/arena.c" -o "$ROOT/build/arena-test"
"$ROOT/build/arena-test"
sh "$ROOT/tests/runtime-primitives.sh"
sh "$ROOT/tests/arena.sh"
sh "$ROOT/tests/json-runtime.sh"
sh "$ROOT/tests/http-runtime.sh"
sh "$ROOT/tests/ir-verifier.sh"
sh "$ROOT/tests/ir-codegen-phi.sh"
sh "$ROOT/tests/diagnostics.sh"
sh "$ROOT/tests/c11-smoke.sh"
printf "RUN: FFI integration tests\n"
sh "$ROOT/tests/ffi.sh"
printf "RUN: multi-source integration tests\n"
sh "$ROOT/tests/multi-source.sh"
sh "$ROOT/tests/no-heap.sh"
sh "$ROOT/tests/utf8.sh"
cc -std=c11 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" "$ROOT/tests/lexer_regressions.c" -o "$ROOT/build/lexer-regressions"
"$ROOT/build/lexer-regressions"
sh "$ROOT/tests/profile.sh"
sh "$ROOT/tests/smoke.sh"
printf 'RUN: regressions\n'
sh "$ROOT/tests/regressions.sh"
printf 'RUN: foundation\n'
sh "$ROOT/tests/foundation.sh"
printf 'RUN: std-library\n'
sh "$ROOT/tests/std-library.sh"
printf 'RUN: raw-memory\n'
sh "$ROOT/tests/raw-memory.sh"
printf 'RUN: std-json\n'
sh "$ROOT/tests/std-json.sh"
printf 'RUN: json-writer\n'
sh "$ROOT/tests/json-writer.sh"
printf 'RUN: http-server\n'
sh "$ROOT/tests/http-server.sh"
printf 'RUN: std-math\n'
sh "$ROOT/tests/std-math.sh"
printf 'RUN: std-collections\n'
sh "$ROOT/tests/std-collections.sh"
printf 'RUN: lazy-imports\n'
sh "$ROOT/tests/lazy-imports.sh"
printf 'RUN: library-scale\n'
sh "$ROOT/tests/library-scale.sh"
printf 'RUN: module-namespaces\n'
sh "$ROOT/tests/module-namespaces.sh"
printf 'RUN: ast-stack-arguments\n'
sh "$ROOT/tests/ast-stack-arguments.sh"
printf 'RUN: architecture\n'
sh "$ROOT/tests/architecture.sh"

cc -std=c11 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" "$ROOT/tests/keywords.c" -o "$ROOT/build/keywords-test"
"$ROOT/build/keywords-test"
