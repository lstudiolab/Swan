#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR="${SWAN_BUILD_DIR:-$ROOT_DIR/build}"
PREFIX="${SWAN_INSTALL_PREFIX:-$HOME/.local}"
BIN_DIR="$PREFIX/bin"

if ! command -v cmake >/dev/null 2>&1; then
    echo "Swan requires CMake 3.20+. Install CMake, then run this script again." >&2
    exit 1
fi

echo "Configuring Swan..."
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

echo "Building Swan..."
cmake --build "$BUILD_DIR" --config Release --parallel

if [ "${SWAN_SKIP_TESTS:-0}" != "1" ]; then
    echo "Running compiler tests..."
    ctest --test-dir "$BUILD_DIR" -C Release --output-on-failure
fi

echo "Installing Swan to $PREFIX..."
cmake --install "$BUILD_DIR" --config Release --prefix "$PREFIX"

if [ ! -x "$BIN_DIR/swan" ]; then
    echo "Installation finished, but $BIN_DIR/swan was not found." >&2
    exit 1
fi

PROFILE=""
case "${SHELL##*/}" in
    zsh) PROFILE="$HOME/.zshrc" ;;
    bash)
        if [ -f "$HOME/.bash_profile" ]; then
            PROFILE="$HOME/.bash_profile"
        else
            PROFILE="$HOME/.bashrc"
        fi
        ;;
esac

PATH_LINE='export PATH="$HOME/.local/bin:$PATH"'
if [ "$PREFIX" = "$HOME/.local" ] && [ -n "$PROFILE" ]; then
    touch "$PROFILE"
    if ! grep -Fqx "$PATH_LINE" "$PROFILE"; then
        printf '\n%s\n' "$PATH_LINE" >> "$PROFILE"
        echo "Added $BIN_DIR to PATH in $PROFILE."
    fi
fi

export PATH="$BIN_DIR:$PATH"
echo ""
echo "Swan is installed."
if command -v swan >/dev/null 2>&1; then
    swan --help 2>/dev/null || true
fi
echo "Try: swan check path/to/file.swan"
if [ -n "$PROFILE" ]; then
    echo "Restart your terminal (or run: . \"$PROFILE\") before using swan in a new session."
else
    echo "Add $BIN_DIR to your PATH to run 'swan' from any directory."
fi
