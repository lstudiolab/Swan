#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SWAN_BIN=${SWAN_BIN:-$ROOT/build/swan}
CC=${CC:-clang}
OPT_LEVEL=${SWAN_OPT_LEVEL:-0}

case "$OPT_LEVEL" in
    0|1|2|3|s|z) ;;
    *)
        echo "compile.sh: SWAN_OPT_LEVEL must be 0, 1, 2, 3, s, or z" >&2
        exit 2
        ;;
esac

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: sh compile.sh <source.swan> [output]" >&2
    exit 2
fi

SOURCE=$1
OUTPUT=${2:-${SOURCE%.swan}}
if [ "$OUTPUT" = "$SOURCE" ]; then
    echo "compile.sh: input must use the .swan extension" >&2
    exit 2
fi

if [ ! -x "$SWAN_BIN" ]; then
    sh "$ROOT/build.sh"
fi
if ! command -v "$CC" >/dev/null 2>&1; then
    echo "compile.sh: C compiler '$CC' was not found" >&2
    exit 127
fi

TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/swan-build.XXXXXX")
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

"$SWAN_BIN" "$SOURCE" -o "$TEMP_DIR/program.c"
"$CC" -std=c11 "-O$OPT_LEVEL" -DNDEBUG "$TEMP_DIR/program.c" -o "$OUTPUT"
