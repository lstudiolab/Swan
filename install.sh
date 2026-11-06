#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
"$ROOT/test.sh"
if [ -n "${PREFIX:-}" ]; then DEST="$PREFIX/bin"; else DEST="$HOME/.local/bin"; fi
mkdir -p "$DEST"
install -m 755 "$ROOT/build/swan" "$DEST/swan"
printf 'Installed Swan to %s/swan\n' "$DEST"
case ":${PATH:-}:" in *":$DEST:"*) ;; *) printf 'Add %s to your PATH.\n' "$DEST" ;; esac
