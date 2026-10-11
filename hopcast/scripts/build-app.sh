#!/usr/bin/env bash
# HopCast - Phase 1: build PRISM's own layer (common-libs + app) on Linux
# against the engine produced by build-engine.sh.
# GPL-2.0-or-later, same as the upstream sources it builds.
#
# Usage: hopcast/scripts/build-app.sh [engine-build-dir] [app-build-dir]
# Needs Qt >= 6.6: set QT_ROOT_DIR (or Qt6_DIR / CMAKE_PREFIX_PATH) to it.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ENGINE_BUILD="$(cd "${1:-$ROOT/build/engine}" && pwd)"
APP_BUILD="${2:-$ROOT/build/app}"
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"

VERSION="$(tr -d '[:space:]' < "$ROOT/hopcast/VERSION")"
IFS=. read -r V_MAJOR V_MINOR V_PATCH V_TWEAK <<< "$VERSION"

# Environment PRISM's CMake expects (mirrors build/mac/01_configure.sh).
export VERSION PLS_VERSION="$VERSION" BUILD_TYPE
export PRISM_VERSION_MAJOR="${V_MAJOR:-0}" PRISM_VERSION_MINOR="${V_MINOR:-0}"
export PRISM_VERSION_PATCH="${V_PATCH:-0}" PRISM_VERSION_TWEAK="${V_TWEAK:-0}"
export PRISM_COMMENTS="HopCast"
export SRC_DIR="$ROOT/src"
export OBS_SRC_DIR="$ROOT/src/obs-studio"
export PRISM_SRC_DIR="$ROOT/src/prism-live-studio"
export OBS_BUILD_DIR="$ENGINE_BUILD"
export PRISM_BUILD_DIR="$APP_BUILD"
export DEV_OUTPUT_DIR="$APP_BUILD/output"
export ARCH="${ARCH:-x86_64}"

QT_PREFIX="${QT_ROOT_DIR:-${QTDIR:-}}"
export QTDIR="$QT_PREFIX"
echo "HopCast app build $VERSION ($BUILD_TYPE), engine: $ENGINE_BUILD, Qt: ${QT_PREFIX:-<system>}"

cmake -S "$ROOT/src" -B "$APP_BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  ${QT_PREFIX:+-DCMAKE_PREFIX_PATH="$QT_PREFIX"} \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF \
  -DENABLE_TEST=OFF \
  -Wno-dev

cmake --build "$APP_BUILD" --parallel "$(nproc)" ${HOPCAST_APP_TARGET:+--target "$HOPCAST_APP_TARGET"}
