#!/usr/bin/env bash
# Compile and run the Phase 0 engine smoke test against an installed engine.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PREFIX="$(cd "${1:-$ROOT/build/engine-install}" && pwd)"
OUT="$ROOT/build/engine-smoke"

cc -O1 -o "$OUT" "$ROOT/hopcast/tests/engine_smoke.c" \
  -I"$PREFIX/include/obs" -L"$PREFIX/lib" -lobs -lX11 -Wl,-rpath,"$PREFIX/lib"

# Mesa software OpenGL lets the renderer start without a GPU (CI).
export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}"

# On Linux, obs_startup() initialises hotkeys, which needs a display server.
# Real desktops have one; on headless CI we run inside a virtual X server.
if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
  if command -v xvfb-run >/dev/null; then
    LD_LIBRARY_PATH="$PREFIX/lib" exec xvfb-run -a -s "-screen 0 1280x720x24" "$OUT" "$PREFIX"
  fi
  echo "No display and xvfb-run not found (install xvfb)" >&2
  exit 1
fi
LD_LIBRARY_PATH="$PREFIX/lib" "$OUT" "$PREFIX"
