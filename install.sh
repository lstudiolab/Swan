#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"
sh "$ROOT/test.sh"
if [ -n "${PREFIX:-}" ]; then
    DEST="$PREFIX/bin"
    DATA="$PREFIX/share/swan"
else
    DEST="$HOME/.local/bin"
    DATA="$HOME/.local/share/swan"
fi
mkdir -p "$DEST" "$DATA/stdlib"
install -m 755 "$ROOT/build/swan" "$DEST/swan-bin"
cp -R "$ROOT/stdlib/." "$DATA/stdlib/"
cat > "$DEST/swan" <<'WRAPPER'
#!/usr/bin/env sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -z "${SWAN_STDLIB:-}" ]; then
    SWAN_STDLIB="$BASE/../share/swan/stdlib"
    export SWAN_STDLIB
fi
exec "$BASE/swan-bin" "$@"
WRAPPER
chmod 755 "$DEST/swan"
printf 'Installed Swan to %s/swan\n' "$DEST"
printf 'Installed standard library to %s/stdlib\n' "$DATA"
case ":${PATH:-}:" in *":$DEST:"*) ;; *) printf 'Add %s to your PATH.\n' "$DEST" ;; esac
