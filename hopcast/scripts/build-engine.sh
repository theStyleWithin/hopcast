#!/usr/bin/env bash
# HopCast - Phase 0: build the pinned engine (NAVER's libobs fork) on Linux.
# GPL-2.0-or-later, same as the upstream sources it builds.
#
# Usage: hopcast/scripts/build-engine.sh [build-dir] [install-prefix]
# Run hopcast/scripts/deps-<distro>.sh first.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ENGINE="$ROOT/src/obs-studio"
BUILD="${1:-$ROOT/build/engine}"
PREFIX="${2:-$ROOT/build/engine-install}"

# NAVER's engine requires the app version via environment (PRISM's own build
# scripts export these). HopCast's version lives in hopcast/VERSION.
VERSION="$(tr -d '[:space:]' < "$ROOT/hopcast/VERSION")"
IFS=. read -r V_MAJOR V_MINOR V_PATCH V_TWEAK <<< "$VERSION"
export PLS_VERSION="$VERSION" VERSION="$VERSION"
export PRISM_VERSION_MAJOR="${V_MAJOR:-0}" PRISM_VERSION_MINOR="${V_MINOR:-0}"
export PRISM_VERSION_PATCH="${V_PATCH:-0}" PRISM_VERSION_TWEAK="${V_TWEAK:-0}"
export PRISM_COMMENTS="HopCast" PRISM_SRC_DIR="$ROOT/src/prism-live-studio" OBS_SRC_DIR="$ROOT/src/obs-studio"
echo "HopCast engine build, version $VERSION"

if [[ ! -f "$ENGINE/libobs/obs.h" ]]; then
  git -C "$ROOT" submodule update --init --depth 1 src/obs-studio
fi
# On Linux the engine's CMake hard-requires these two submodule checkouts,
# even with ENABLE_BROWSER/ENABLE_WEBSOCKET=OFF.
for sm in plugins/obs-browser plugins/obs-websocket; do
  [[ -f "$ENGINE/$sm/CMakeLists.txt" ]] || git -C "$ENGINE" submodule update --init --depth 1 "$sm"
done

cmake -S "$ENGINE" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DENABLE_UI=OFF \
  -DENABLE_RELOCATABLE=ON \
  -DENABLE_PORTABLE_CONFIG=ON \
  -DENABLE_BROWSER=OFF \
  -DENABLE_WEBSOCKET=OFF \
  -DENABLE_WEBRTC=OFF \
  -DENABLE_AJA=OFF \
  -DENABLE_DECKLINK=OFF \
  -DENABLE_VLC=OFF \
  -DENABLE_SCRIPTING=OFF \
  -DENABLE_COMPAT_UPDATES=OFF \
  -DENABLE_SERVICE_UPDATES=OFF \
  -DENABLE_PIPEWIRE=ON \
  -DENABLE_PULSEAUDIO=ON \
  -DENABLE_V4L2=ON \
  -DENABLE_WAYLAND=ON

cmake --build "$BUILD" --parallel "$(nproc)"
cmake --install "$BUILD"

echo
echo "Engine installed to: $PREFIX"
ls "$PREFIX"/lib/libobs.so* 2>/dev/null || true
