#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
INCLUDE="$ROOT/compiler/include/swan"

cpp_files=$(find "$ROOT" -type f -name '*.cpp' ! -path "$ROOT/build/*" -print | sort)
if [ "$cpp_files" != "$ROOT/main.cpp" ]; then
    echo "FAIL: expected main.cpp to be the only C++ translation unit" >&2
    printf '%s\n' "$cpp_files" >&2
    exit 1
fi

reject_dependency() {
    subsystem=$1
    forbidden=$2
    directory="$INCLUDE/$subsystem"

    if grep -R -n -E "^[[:space:]]*#include[[:space:]]*[<\"]swan/($forbidden)/" "$directory"; then
        echo "FAIL: $subsystem depends on a higher-level subsystem" >&2
        exit 1
    fi
}

reject_dependency Basic 'Support|Lex|AST|Parse|Sema|Frontend|CodeGen|Compiler|Driver'
reject_dependency Support 'Lex|AST|Parse|Sema|Frontend|CodeGen|Compiler|Driver'
reject_dependency Lex 'AST|Parse|Sema|Frontend|CodeGen|Compiler|Driver'
reject_dependency AST 'Lex|Parse|Sema|Frontend|CodeGen|Compiler|Driver'
reject_dependency Parse 'Sema|Frontend|CodeGen|Compiler|Driver'
reject_dependency Sema 'Frontend|CodeGen|Compiler|Driver'
reject_dependency Frontend 'CodeGen|Compiler|Driver'
reject_dependency CodeGen 'Frontend|Compiler|Driver'
reject_dependency Compiler 'Driver'

printf 'PASS: compiler architecture boundaries\n'
