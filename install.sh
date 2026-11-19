#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"
sh "$ROOT/test.sh"
if [ -n "${PREFIX:-}" ]; then
    DEST="$PREFIX/bin"
    DATA="$PREFIX/share/brew"
else
    DEST="$HOME/.local/bin"
    DATA="$HOME/.local/share/brew"
fi
mkdir -p "$DEST" "$DATA/std"
install -m 755 "$ROOT/build/brewc" "$DEST/brewc-bin"
cp -R "$ROOT/std/." "$DATA/std/"
cat > "$DEST/brewc" <<'WRAPPER'
#!/usr/bin/env sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -z "${BREW_STDLIB:-}" ]; then
    BREW_STDLIB="$BASE/../share/brew/std"
    export BREW_STDLIB
fi
exec "$BASE/brewc-bin" "$@"
WRAPPER
chmod 755 "$DEST/brewc"
printf 'Installed brewc to %s/brewc\n' "$DEST"
printf 'Installed standard library to %s/std\n' "$DATA"
case ":${PATH:-}:" in *":$DEST:"*) ;; *) printf 'Add %s to your PATH.\n' "$DEST" ;; esac

