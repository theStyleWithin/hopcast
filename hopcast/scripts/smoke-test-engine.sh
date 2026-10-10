#!/usr/bin/env bash
# Compile and run the Phase 0 engine smoke test against an installed engine.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PREFIX="$(cd "${1:-$ROOT/build/engine-install}" && pwd)"
OUT="$ROOT/build/engine-smoke"

cc -O1 -o "$OUT" "$ROOT/hopcast/tests/engine_smoke.c" \
  -I"$PREFIX/include/obs" -L"$PREFIX/lib" -lobs -Wl,-rpath,"$PREFIX/lib"

# No display in CI: libobs core + plugin loading do not need one.
LD_LIBRARY_PATH="$PREFIX/lib" "$OUT" "$PREFIX"
