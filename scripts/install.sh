#!/usr/bin/env sh
set -eu

REPO="lstudiolab/Swan"
VERSION="${SWAN_VERSION:-latest}"
BIN_DIR="${SWAN_INSTALL_DIR:-$HOME/.local/bin}"
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
  URL="https://github.com/$REPO/releases/latest/download/swan-$ASSET.tar.gz"
else
  URL="https://github.com/$REPO/releases/download/$VERSION/swan-$ASSET.tar.gz"
fi

if command -v curl >/dev/null 2>&1; then
  curl -fL "$URL" -o "$TMP_DIR/swan.tar.gz"
elif command -v wget >/dev/null 2>&1; then
  wget -O "$TMP_DIR/swan.tar.gz" "$URL"
else
  echo "Install curl or wget first." >&2
  exit 1
fi

tar -xzf "$TMP_DIR/swan.tar.gz" -C "$TMP_DIR"
mkdir -p "$BIN_DIR"
install -m 755 "$TMP_DIR/swan/bin/swanc" "$BIN_DIR/swanc"
ln -sf "$BIN_DIR/swanc" "$BIN_DIR/swan"

echo "Installed Swan to $BIN_DIR"
case ":$PATH:" in
  *":$BIN_DIR:"*) ;;
  *) echo "Add this to your shell profile: export PATH=\"$BIN_DIR:\$PATH\"" ;;
esac
echo "Run: swan --version"
