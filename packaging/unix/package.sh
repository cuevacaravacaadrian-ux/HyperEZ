#!/bin/sh
# Crea el paquete de Linux o macOS: dist/HyperEZ-<versión>-<sistema>-<arquitectura>.tar.gz
#   sh packaging/unix/package.sh build/hyper [extension.vsix]
set -e
HYPER="$1"
VSIX="$2"
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
[ -x "$HYPER" ] || { echo "uso: package.sh <ruta de hyper> [extension.vsix]"; exit 2; }

VERSION=$("$HYPER" version | head -n 1 | sed 's/HyperEZ //')
case "$(uname)" in
    Darwin) OS=macos ;;
    *) OS=linux ;;
esac
case "$(uname -m)" in
    x86_64|amd64) ARCH=x64 ;;
    arm64|aarch64) ARCH=arm64 ;;
    *) ARCH=$(uname -m) ;;
esac
NAME="HyperEZ-$VERSION-$OS-$ARCH"
STAGE="$ROOT/build-package/$NAME"
rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/share/hyperez/docs"
cp "$HYPER" "$STAGE/bin/hyper"
strip "$STAGE/bin/hyper" 2>/dev/null || true
cp -R "$ROOT/examples" "$STAGE/share/hyperez/examples"
find "$STAGE/share/hyperez/examples" -name '*.out' -delete
cp -R "$ROOT/docs/." "$STAGE/share/hyperez/docs/"
"$HYPER" docs "$STAGE/share/hyperez/docs/API.md" >/dev/null 2>&1 || true
cp "$ROOT/assets/icons/hyperez-256.png" "$STAGE/share/hyperez/hyperez.png"
cp "$ROOT/packaging/unix/install.sh" "$STAGE/install.sh"
chmod +x "$STAGE/install.sh" "$STAGE/bin/hyper"
cp "$ROOT/README.md" "$ROOT/LICENSE" "$ROOT/CHANGELOG.md" "$ROOT/THIRD-PARTY-NOTICES.md" "$ROOT/assets/fonts/LICENSE-DejaVu.txt" "$STAGE/"
[ -n "$VSIX" ] && [ -f "$VSIX" ] && cp "$VSIX" "$STAGE/"

mkdir -p "$ROOT/dist"
tar -C "$ROOT/build-package" -czf "$ROOT/dist/$NAME.tar.gz" "$NAME"
echo "dist/$NAME.tar.gz"
