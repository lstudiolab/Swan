#!/usr/bin/env sh
set -eu

REPO="lstudiolab/Brew"
VERSION="${BREW_VERSION:-latest}"
BIN_DIR="${BREW_INSTALL_DIR:-$HOME/.local/bin}"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

OS="$(uname -s)"
ARCH="$(uname -m)"
case "$OS:$ARCH" in
  Linux:x86_64|Linux:amd64) ASSET="linux-x86_64" ;;
  Linux:aarch64|Linux:arm64) ASSET="linux-aarch64" ;;
  Darwin:x86_64) ASSET="macos-x86_64" ;;
  Darwin:arm64|Darwin:aarch64) ASSET="macos-aarch64" ;;
  *) echo "Unsupported platform: $OS/$ARCH" >&2; exit 1 ;;
esac

if [ "$VERSION" = "latest" ]; then
  URL="https://github.com/$REPO/releases/latest/download/brew-$ASSET.tar.gz"
else
  URL="https://github.com/$REPO/releases/download/$VERSION/brew-$ASSET.tar.gz"
fi

if command -v curl >/dev/null 2>&1; then
  curl -fL "$URL" -o "$TMP_DIR/brew.tar.gz"
elif command -v wget >/dev/null 2>&1; then
  wget -O "$TMP_DIR/brew.tar.gz" "$URL"
else
  echo "Install curl or wget first." >&2
  exit 1
fi

tar -xzf "$TMP_DIR/brew.tar.gz" -C "$TMP_DIR"
mkdir -p "$BIN_DIR"
install -m 755 "$TMP_DIR/brew/bin/brewc" "$BIN_DIR/brewc"
ln -sf "$BIN_DIR/brewc" "$BIN_DIR/brew"

echo "Installed Brew to $BIN_DIR"
case ":$PATH:" in
  *":$BIN_DIR:"*) ;;
  *) echo "Add this to your shell profile: export PATH=\"$BIN_DIR:\$PATH\"" ;;
esac
echo "Run: brew --version"
