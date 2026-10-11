#!/usr/bin/env bash
# HopCast - Phase 1 launch test: start the built app inside a virtual X server,
# let it run for a while, take a screenshot, then stop it and collect logs.
# GPL-2.0-or-later.
#
# Usage: hopcast/scripts/launch-test-app.sh [stage-dir] [out-dir] [seconds]
# stage-dir is the installed tree from build-app.sh (bin/PRISMLiveStudio).
# Exit status: 0 if the app was still running when the screenshot was taken
# (i.e. it did not crash on startup), 1 otherwise.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STAGE="$(cd "${1:-$ROOT/build/stage}" && pwd)"
OUT="$(mkdir -p "${2:-$ROOT/build/launch-test}" && cd "${2:-$ROOT/build/launch-test}" && pwd)"
SECS="${3:-30}"

BIN="$STAGE/bin/PRISMLiveStudio"
[[ -x "$BIN" ]] || { echo "PRISMLiveStudio binary not found at $BIN" >&2; exit 1; }
echo "Launching $BIN for ${SECS}s"

export LIBGL_ALWAYS_SOFTWARE=1 QT_QPA_PLATFORM=xcb
export XDG_CONFIG_HOME="$OUT/config" XDG_DATA_HOME="$OUT/data" XDG_CACHE_HOME="$OUT/cache"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME"

# Libraries resolve through RPATH ($ORIGIN/../lib); Qt comes from the environment.
find "$STAGE" -maxdepth 3 -not -path '*/share/*' | sort > "$OUT/stage-tree.txt"
ldd "$BIN" > "$OUT/ldd.txt" 2>&1 || true
grep "not found" "$OUT/ldd.txt" && echo "::warning title=launch::unresolved libraries (see ldd.txt)"

Xvfb :99 -screen 0 1600x900x24 >/dev/null 2>&1 &
XVFB=$!
export DISPLAY=:99
sleep 2

# Run under gdb when available so a startup crash leaves a backtrace.
if command -v gdb >/dev/null; then
  ( cd "$(dirname "$BIN")" && exec gdb -q -batch -return-child-result \
      -ex "set pagination off" -ex "handle SIGPIPE nostop noprint" -ex run \
      -ex "echo \\n=== CRASH BACKTRACE ===\\n" -ex bt \
      -ex "echo \\n=== ALL THREADS ===\\n" -ex "thread apply all bt 25" \
      --args "$BIN" ) > "$OUT/stdout.txt" 2>&1 &
else
  ( cd "$(dirname "$BIN")" && exec "$BIN" ) > "$OUT/stdout.txt" 2>&1 &
fi
APP=$!

alive=1
for ((i = 0; i < SECS; i++)); do
  sleep 1
  kill -0 "$APP" 2>/dev/null || { alive=0; break; }
done

xwd -root -display :99 -silent > "$OUT/screen.xwd" 2>/dev/null \
  && convert "$OUT/screen.xwd" "$OUT/screen.png" 2>/dev/null && rm -f "$OUT/screen.xwd"
xwininfo -root -tree -display :99 > "$OUT/windows.txt" 2>&1 || true

if (( alive )); then
  # Interrupt the app under gdb: gdb then prints every thread's stack (a hang shows up here).
  if inferior=$(pgrep -P "$APP" | head -n1) && [[ -n "$inferior" ]]; then
    echo "still running after ${SECS}s; interrupted pid $inferior for stacks" > "$OUT/hang.txt"
    kill -INT "$inferior" 2>/dev/null
    for ((i = 0; i < 20; i++)); do sleep 1; kill -0 "$APP" 2>/dev/null || break; done
  fi
  echo "App still running after ${SECS}s - stopping it"
  kill -TERM "$APP" 2>/dev/null; sleep 3; kill -KILL "$APP" 2>/dev/null
  status=0
else
  wait "$APP"; code=$?
  echo "App exited early with status $code"
  echo "$code" > "$OUT/exit-code.txt"
  status=1
fi
kill "$XVFB" 2>/dev/null

# PRISM/OBS write their own logs under the config dir.
find "$OUT/config" "$OUT/data" "$OUT/cache" -type f > "$OUT/files.txt" 2>/dev/null
find "$OUT/config" "$OUT/data" -type f \( -name '*.txt' -o -name '*.log' \) -size -2M \
  -exec sh -c 'cp "$1" "$2/applog-$(basename "$1")"' _ {} "$OUT" \; 2>/dev/null
echo "--- stdout/stderr ---"; sed -n "/CRASH BACKTRACE/,/ALL THREADS/p" "$OUT/stdout.txt"; tail -n 40 "$OUT/stdout.txt"
# HopCast must never reach NAVER: report blocked attempts and any request that got through.
blocked=$(grep -c "HopCast: blocked request" "$OUT/stdout.txt" || true)
leaked=$(grep -E "http response (success|failed), url = https?://[^ ]*(naver\.(com|net)|navercorp\.com|pstatic\.net|prismlive\.com|ncloud\.com)" "$OUT/stdout.txt" | grep -v "hopcast-blocked" | head -5)
echo "NAVER requests blocked before sending: $blocked" | tee "$OUT/naver-check.txt"
if [[ -n "$leaked" ]]; then
  echo "::error title=NAVER traffic::$leaked"; echo "LEAKED: $leaked" >> "$OUT/naver-check.txt"; status=1
fi
echo "--- top-level windows ---"; grep -E '^\s+0x[0-9a-f]+ "' "$OUT/windows.txt" | head -20
exit $status
