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
rm -rf "$OUT/home" # every tour starts from a fresh profile
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
  # Stop every HopCast process (PRISM also starts helper processes that would otherwise
  # keep the single-instance lock), and drop the crash marker so the next start is normal.
  pkill -KILL -f "$STAGE/bin/" 2>/dev/null; wait "$APP" 2>/dev/null
  for _ in $(seq 1 10); do pgrep -f "$STAGE/bin/" >/dev/null || break; sleep 0.5; done
  rm -rf "$XDG_CONFIG_HOME/PRISMLiveStudio/.sentinel"
  sleep 1
done < "$SCENARIOS"

kill "$COMP" "$XVFB" 2>/dev/null
echo "$n scenarios captured in $OUT"
