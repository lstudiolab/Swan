#!/usr/bin/env sh
set -eu
ROOT_DIR=$(CDPATH= cd "$(dirname "$0")" && pwd)
PYTHON="${PYTHON:-python3}"
command -v "$PYTHON" >/dev/null 2>&1 || { echo "Swan requires Python 3.10 or newer." >&2; exit 1; }
"$PYTHON" -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 10) else 1)' || { echo "Swan requires Python 3.10 or newer." >&2; exit 1; }
echo "Running Swan compiler tests..."
PYTHONPATH="$ROOT_DIR/Compiler/Python" "$PYTHON" -m unittest discover -s "$ROOT_DIR/Compiler/Python/tests" -v
echo "Installing Swan..."
"$PYTHON" -m pip install --user "$ROOT_DIR/Compiler/Python"
echo "Swan is installed. Try: swan path/to/program.swan -o program.c"
echo "The compiler emits C source only; it does not compile or run the C program."
