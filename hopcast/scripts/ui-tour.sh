#!/usr/bin/env bash
# HopCast UI tour: start the staged app on a virtual screen, click through the scenarios in
# hopcast/tests/ui-tour.txt and save a screenshot (plus window list) for each.
# Usage: hopcast/scripts/ui-tour.sh [stage-dir] [out-dir] [scenario-file]
# Needs: Xvfb, xdotool, xcompmgr (draws translucent windows), ImageMagick. GPL-2.0-or-later.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STAGE="$(cd "${1:-$ROOT/build/stage}" && pwd)"
OUT="$(mkdir -p "${2:-$ROOT/build/ui-tour}" && cd "${2:-$ROOT/build/ui-tour}" && pwd)"
SCENARIOS="${3:-$ROOT/hopcast/tests/ui-tour.txt}"
BIN="$STAGE/bin/PRISMLiveStudio"

export LIBGL_ALWAYS_SOFTWARE=1 QT_QPA_PLATFORM=xcb DISPLAY=:98
export XDG_CONFIG_HOME="$OUT/home/config" XDG_DATA_HOME="$OUT/home/data" XDG_CACHE_HOME="$OUT/home/cache"
export HOME="$OUT/home" # folder pickers etc. start here
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$HOME/Documents"

Xvfb :98 -screen 0 1600x900x24 >/dev/null 2>&1 & XVFB=$!
sleep 2
xcompmgr -n >/dev/null 2>&1 & COMP=$!

shot() { import -window root "$OUT/$1.png" 2>/dev/null; xwininfo -root -tree > "$OUT/$1.windows.txt" 2>&1; echo "  shot $1"; }

run_action() {
  local verb rest; read -r verb rest <<< "$1"
  case "$verb" in
    click)  xdotool mousemove $rest click 1 ;;
    rclick) xdotool mousemove $rest click 3 ;;
    key)    xdotool key --delay 80 $rest ;;
    type)   xdotool type --delay 30 "$rest" ;;
    wait)   sleep "$rest" ;;
    shot)   shot "$rest" ;;
    "")     ;;
    *)      echo "  unknown action: $1" ;;
  esac
}

n=0
while IFS= read -r line; do
  [[ -z "${line// }" || "$line" == \#* ]] && continue
  name="$(echo "${line%%|*}" | xargs)"; actions="${line#*|}"
  n=$((n + 1)); printf -v name '%02d-%s' "$n" "$name"
  echo "scenario $name"
  ( cd "$STAGE/bin" && exec "$BIN" ) >> "$OUT/app.log" 2>&1 & APP=$!
  for _ in $(seq 1 60); do xdotool search --name '^HopCast$' >/dev/null 2>&1 && break; sleep 1; done
  sleep 4 # let the studio finish laying out
  IFS=';' read -ra steps <<< "$actions"
  for step in "${steps[@]}"; do run_action "$(echo "$step" | xargs)"; done
  shot "$name"
  # Quit cleanly so the next start has no "didn't shut down properly" prompt:
  # close open menus/dialogs, then File > Exit (Shift+X), confirming if asked.
  xdotool key Escape; sleep 0.5; xdotool key Escape; sleep 0.5
  xdotool key shift+x; sleep 3
  kill -0 "$APP" 2>/dev/null && { xdotool key Return; sleep 3; }
  for _ in $(seq 1 10); do kill -0 "$APP" 2>/dev/null || break; sleep 1; done
  if kill -0 "$APP" 2>/dev/null; then
    echo "  (had to stop the app)"; kill -TERM "$APP"; sleep 3; kill -KILL "$APP" 2>/dev/null
  fi
  wait "$APP" 2>/dev/null
  rm -rf "$XDG_CONFIG_HOME/PRISMLiveStudio/.sentinel" # crash marker, if any
done < "$SCENARIOS"

kill "$COMP" "$XVFB" 2>/dev/null
echo "$n scenarios captured in $OUT"
