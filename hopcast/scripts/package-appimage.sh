#!/usr/bin/env bash
# HopperCast - package the staged install tree as an AppImage.
# Usage: package-appimage.sh <stage-dir> <out-dir> <release|debug>
# Needs Qt (qmake in PATH), gcc, imagemagick, and network access for linuxdeploy. GPL-2.0-or-later.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STAGE="$(cd "$1" && pwd)"; OUT="$(mkdir -p "$2" && cd "$2" && pwd)"; VARIANT="${3:-release}"
VERSION="$(tr -d '[:space:]' < "$ROOT/hopcast/VERSION")"
TOOLS="$OUT/tools"; mkdir -p "$TOOLS"
export APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64

get() { [[ -x "$TOOLS/$1" ]] || { curl -fsSL -o "$TOOLS/$1" "$2" && chmod +x "$TOOLS/$1"; }; }
get linuxdeploy https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
get linuxdeploy-plugin-qt https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage

APPDIR="$OUT/HopperCast-$VARIANT.AppDir"; rm -rf "$APPDIR"; mkdir -p "$APPDIR/usr"
cp -a "$STAGE"/. "$APPDIR/usr/"
mkdir -p "$APPDIR/usr/share/hopcast" "$APPDIR/apprun-hooks"
cp "$ROOT/hopcast/VERSION" "$APPDIR/usr/share/hopcast/VERSION"

NAME="HopperCast"
[[ $VARIANT == debug ]] && NAME="HopperCast (Debug)"
cat > "$OUT/hoppercast.desktop" <<D
[Desktop Entry]
Type=Application
Name=$NAME
Comment=Live streaming and recording studio
Exec=PRISMLiveStudio
Icon=hoppercast
Categories=AudioVideo;Video;Recorder;
Terminal=false
StartupWMClass=PRISMLiveStudio
D
ICON="$ROOT/hopcast/appimage/hoppercast.png"
if [[ ! -f $ICON ]]; then
  convert -size 256x256 xc:'#1d2a3a' -fill '#f2b705' -draw 'circle 128,128 128,40' \
    -fill '#1d2a3a' -draw 'polygon 104,92 104,164 172,128' "$OUT/hoppercast.png"; ICON="$OUT/hoppercast.png"
fi

if [[ $VARIANT == debug ]]; then
  mkdir -p "$APPDIR/usr/lib/hopcast"
  gcc -shared -fPIC -O1 -o "$APPDIR/usr/lib/hopcast/libhopcast-crash.so" "$ROOT/hopcast/appimage/crash-handler.c"
  cp "$ROOT/hopcast/appimage/debug-hook.sh" "$APPDIR/apprun-hooks/hopcast-debug.sh"
  export NO_STRIP=1
fi

# Plugins are dlopen()ed, so give their dependencies to linuxdeploy explicitly.
LIBS=(); while IFS= read -r f; do LIBS+=(--library "$f"); done < <(find "$APPDIR/usr/lib" -name '*.so*' -path '*plugins*' -type f 2>/dev/null)

export PATH="$TOOLS:$PATH" QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake)}"
export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so;libqwayland-egl.so"
export EXTRA_QT_PLUGINS="svg;imageformats;tls;networkinformation;wayland-decoration-client;wayland-graphics-integration-client;wayland-shell-integration;xcbglintegrations;platformthemes;multimedia"
export LDAI_OUTPUT="$OUT/HopperCast-$VARIANT-$VERSION-x86_64.AppImage"
cd "$OUT"
linuxdeploy --appdir "$APPDIR" "${LIBS[@]}" \
  --executable "$APPDIR/usr/bin/PRISMLiveStudio" \
  --desktop-file "$OUT/hoppercast.desktop" --icon-file "$ICON" --icon-filename hoppercast \
  --plugin qt --output appimage
ls -l "$OUT"/*.AppImage
