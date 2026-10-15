#!/usr/bin/env bash
# HopCast runtime audit: exercise the staged app the way a person on a real desktop would,
# and record anything that goes wrong. Things the UI tour never does:
#   - a sound server is running (PulseAudio null sink), like PipeWire on a desktop
#   - stream to a real RTMP server (ffmpeg on 127.0.0.1) and check video arrived
#   - record to disk and check the file plays
#   - quit normally (Ctrl+Q) instead of being killed, and check the exit status
#   - start again on the same profile (saved channel, encrypted store, scenes)
#   - start on Wayland (weston, headless)
#   - every .so resolves its libraries
# Usage: hopcast/scripts/audit-runtime.sh [stage-dir] [out-dir]
# Env: AUDIT_SLOW=N multiplies waits (sanitizer builds are slower); AUDIT_GDB=0 disables gdb.
# Needs: Xvfb xdotool xcompmgr imagemagick ffmpeg pulseaudio weston gdb. GPL-2.0-or-later.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STAGE="$(cd "${1:-$ROOT/build/stage}" && pwd)"
OUT="$(mkdir -p "${2:-$ROOT/build/audit}" && cd "${2:-$ROOT/build/audit}" && pwd)"
BIN="$STAGE/bin/PRISMLiveStudio"
SLOW="${AUDIT_SLOW:-1}"
SUMMARY="$OUT/summary.txt"
: > "$SUMMARY"
START_MARK="$OUT/.start-marker"; touch "$START_MARK"

note() { echo "$*" | tee -a "$SUMMARY"; }
pass() { note "PASS  $*"; }
fail() { note "FAIL  $*"; }
info() { note "INFO  $*"; }
w() { sleep "$(awk -v a="$1" -v s="$SLOW" 'BEGIN{print a*s}')"; }

# ---------------------------------------------------------------- 1. libraries
: > "$OUT/missing-libs.txt"
while IFS= read -r f; do
  ldd "$f" 2>/dev/null | grep "not found" | sed "s|^|${f#$STAGE/}: |" >> "$OUT/missing-libs.txt"
done < <(find "$STAGE" -type f \( -name '*.so' -o -name '*.so.*' \) ; echo "$BIN")
n=$(sort -u "$OUT/missing-libs.txt" | wc -l)
(( n == 0 )) && pass "every binary and plugin resolves its libraries" || fail "$n unresolved library references (missing-libs.txt)"

QTP="${QT_ROOT_DIR:-}/plugins"
ls "$QTP/platforms" > "$OUT/qt-platform-plugins.txt" 2>&1 || true
if grep -q wayland "$OUT/qt-platform-plugins.txt"; then pass "Qt has a native Wayland platform plugin"
else fail "Qt has no Wayland platform plugin: on Wayland desktops the app would need XWayland"; fi

# ---------------------------------------------------------------- environment
export LIBGL_ALWAYS_SOFTWARE=1
export HOME="$OUT/home" XDG_CONFIG_HOME="$OUT/home/.config" XDG_DATA_HOME="$OUT/home/.local/share" XDG_CACHE_HOME="$OUT/home/.cache"
export XDG_RUNTIME_DIR="$OUT/runtime"
rm -rf "$HOME" "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$HOME/Documents" "$HOME/Videos"
mkdir -m 0700 -p "$XDG_RUNTIME_DIR"
SHOTS="$OUT/shots"; mkdir -p "$SHOTS"

# A sound server, as on any desktop (PipeWire answers the same PulseAudio protocol).
if command -v pulseaudio >/dev/null; then
  pulseaudio -n --daemonize=yes --exit-idle-time=-1 --log-target=file:"$OUT/pulse.log" \
    -L "module-native-protocol-unix" -L "module-null-sink sink_name=hopcast_speakers" \
    -L "module-virtual-source source_name=hopcast_mic master=hopcast_speakers.monitor" 2>>"$OUT/pulse.log"
  sleep 2
  pactl info > "$OUT/pulse-info.txt" 2>&1 && info "PulseAudio running (null speakers + virtual mic)" || fail "PulseAudio did not start (pulse.log)"
fi

# Wrap the recording helper so its arguments, error output and exit code are kept
# (libobs only shows its stderr when a pipe write fails).
MUX="$STAGE/bin/obs-ffmpeg-mux"
if [[ -x "$MUX" && ! -e "$MUX.real" ]]; then
  mv "$MUX" "$MUX.real"
  cat > "$MUX" <<WRAP
#!/usr/bin/env bash
log="$OUT/ffmpeg-mux-\$(date +%H%M%S)-\$\$.log"
{ echo "argc=\$#"; for a in "\$@"; do printf '[%s]\\n' "\$a"; done; } > "\$log"
# keep the first 256 KB of what OBS sends (packet framing + codec headers)
tee -p >(head -c 262144 > "\$log.stdin") | "$MUX.real" "\$@" 2>>"\$log"
code=\${PIPESTATUS[1]}
echo "exit=\$code" >> "\$log"
exit \$code
WRAP
  chmod +x "$MUX"
fi

# Detector for code that closes the app's stdin/stdout/stderr (first run only).
command -v gcc >/dev/null && gcc -shared -fPIC -O1 -o "$OUT/fd0-guard.so" "$ROOT/hopcast/tests/fd0-guard.c" -ldl 2>"$OUT/fd0-guard-build.txt" || true
rm -f "$OUT/fd-guard.log"

APPLOG="$OUT/app.log"
GDB=0; [[ "${AUDIT_GDB:-1}" == 1 ]] && command -v gdb >/dev/null && GDB=1
start_app() { # $1 = label; app output goes to $OUT/app-<label>.log
  local log="$OUT/app-$1.log" gs="$OUT/.gdb-$1"
  if (( GDB )); then
    {
      echo "set pagination off"
      echo "handle SIGPIPE nostop noprint"; echo "handle SIG32 nostop noprint"; echo "handle SIG34 nostop noprint"
      # Who forks the (multi-threaded) app? fork() there can deadlock in the child.
      if [[ "$1" == first || "$1" == second ]]; then
        echo "catch fork"; echo "catch vfork"
        printf 'commands 1 2\nsilent\nprintf "=== fork ===\\n"\nbt 20\ncontinue\nend\n'
      fi
      # The close() guard can't be preloaded ahead of the ASan runtime.
      if [[ "$1" == first && -f "$OUT/fd0-guard.so" && -z "${ASAN_OPTIONS:-}" ]]; then # who closes the app's stdin/stdout/stderr?
        echo "set environment LD_PRELOAD $OUT/fd0-guard.so"
        echo "set environment HOPCAST_FD_GUARD_LOG $OUT/fd-guard.log"
        echo "set environment HOPCAST_FD_GUARD_EXE PRISMLiveStudio"
      fi
      echo "run"
      # Only after a crash or an interrupt: a normal exit leaves no stack, and 'bt' would make gdb exit 1.
      echo 'if $_isvoid($_exitcode)'
      echo '  echo \n=== CRASH BACKTRACE ===\n'
      echo '  bt'
      echo '  echo \n=== ALL THREADS ===\n'
      echo '  thread apply all bt 20'
      echo 'end'
    } > "$gs"
    ( cd "$STAGE/bin" && exec gdb -q -batch -return-child-result -x "$gs" --args "$BIN" ) > "$log" 2>&1 &
  else
    ( cd "$STAGE/bin" && exec "$BIN" ) > "$log" 2>&1 &
  fi
  APP=$!
}
wait_window() { # wait until the HopCast main window is visible (X11)
  for _ in $(seq 1 $((90 * SLOW))); do xdotool search --onlyvisible --name '^HopCast$' >/dev/null 2>&1 && return 0; kill -0 "$APP" 2>/dev/null || return 1; sleep 1; done
  return 1
}
shot() { import -window root "$SHOTS/$1.png" 2>/dev/null; }
click() { xdotool mousemove "$1" "$2" click 1; }
quit_app() { # Ctrl+Q on the main window; returns the exit status (124 = did not exit)
  local win; win=$(xdotool search --onlyvisible --name '^HopCast$' 2>/dev/null | head -1)
  [[ -n "$win" ]] && xdotool windowactivate --sync "$win" 2>/dev/null
  xdotool key --clearmodifiers ctrl+q; w 3; shot "$1-quit-1"
  for _ in $(seq 1 $((40 * SLOW))); do kill -0 "$APP" 2>/dev/null || break; sleep 1; done
  if kill -0 "$APP" 2>/dev/null; then # a confirmation dialog? accept it once
    xdotool key Return; w 3; shot "$1-quit-2"
    for _ in $(seq 1 $((30 * SLOW))); do kill -0 "$APP" 2>/dev/null || break; sleep 1; done
  fi
  if kill -0 "$APP" 2>/dev/null; then
    local inf; inf=$(pgrep -P "$APP" | head -1); [[ -n "$inf" ]] && kill -INT "$inf" 2>/dev/null; sleep 5
    pkill -KILL -f "$STAGE/bin/" 2>/dev/null; wait "$APP" 2>/dev/null; return 124
  fi
  wait "$APP"; return $?
}
# A real crash: the app stopped on a fatal signal (our own SIGINT for hang stacks does not count).
crashed() { grep -qE "received signal SIG(SEGV|ABRT|BUS|FPE|ILL|TRAP)" "$1"; }

Xvfb :97 -screen 0 1600x900x24 >/dev/null 2>&1 & XVFB=$!
export DISPLAY=:97 QT_QPA_PLATFORM=xcb
sleep 2
xcompmgr -n >/dev/null 2>&1 & COMP=$!

# ---------------------------------------------------------------- 2. first run: channel, stream, record, quit
RTMP_URL="rtmp://127.0.0.1:1935/live"; RTMP_KEY="hopcast-audit"
ffmpeg -nostdin -hide_banner -loglevel info -listen 1 -rw_timeout 180000000 -f flv \
  -i "$RTMP_URL/$RTMP_KEY" -c copy -t 25 -y "$OUT/received.flv" > "$OUT/rtmp-server.log" 2>&1 & RTMP=$!

start_app first
if wait_window; then pass "first start: main window shown"; else fail "first start: main window never appeared"; fi
w 5; shot 01-first-start
fdsnap() { local pid; pid=$(pgrep -x PRISMLiveStudio | head -1); [[ -n "$pid" ]] && ls -l /proc/$pid/fd 2>/dev/null | awk '{print $9, $10, $11}' | sort -n > "$OUT/fds-$1.txt"; }
fdsnap start

# Add a Custom RTMP channel pointing at the local server (same clicks as the UI tour).
click 216 62; w 3
click 880 273; xdotool type --delay 20 "$RTMP_URL"
click 880 323; xdotool type --delay 20 "$RTMP_KEY"
click 880 445; xdotool type --delay 20 "Audit Server"
w 1; shot 02-channel-form
click 942 763; w 3; shot 03-storage-prompt
xdotool key Return; w 3; xdotool key Escape; w 3   # "Choose Folder…" then cancel = HopCast's folder
shot 04-channel-added
ls "$XDG_CONFIG_HOME"/PRISMLiveStudio/user/hopcast-rtmp-channels.enc >/dev/null 2>&1 \
  && pass "stream key saved encrypted" || fail "encrypted stream-key file not written"
grep -rl "$RTMP_KEY" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" 2>/dev/null > "$OUT/plaintext-key-files.txt"
[[ -s "$OUT/plaintext-key-files.txt" ]] && fail "stream key found in plain text (plaintext-key-files.txt)" || pass "stream key not stored in plain text anywhere in the profile"

# Mixer should list audio devices now that a sound server runs.
shot 05-mixer

# Record 8 s before any streaming (compared with the recording after the stream).
click 1460 873; w 3; click 724 557; w 8; click 1460 873; w 5; shot 05b-record-before-live
click 798 527; w 2
pre=$(find "$HOME" -maxdepth 1 -type f -name '*.mkv' -newer "$START_MARK" 2>/dev/null | head -1)
[[ -n "$pre" ]] && pass "recording before going live: file written" || fail "recording before going live: no file"
touch "$START_MARK"

# Go live.
click 1328 873; w 3; shot 06-go-live-1
click 724 557; w 6; shot 07-go-live-2       # "no video sources" -> Yes
w 15; shot 08-live
grep -rl "$RTMP_KEY" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" 2>/dev/null > "$OUT/plaintext-key-files-live.txt"
[[ -s "$OUT/plaintext-key-files-live.txt" ]] && fail "while live, stream key in plain text: $(sed "s|$HOME/||" "$OUT/plaintext-key-files-live.txt" | tr '\n' ' ')" || pass "while live, stream key not in plain text anywhere in the profile"
grep -l "$RTMP_KEY" "$OUT/app-first.log" >/dev/null 2>&1 && info "stream key appears in the app's own log output (check log masking)"
for _ in $(seq 1 $((30 * SLOW))); do kill -0 "$RTMP" 2>/dev/null || break; sleep 1; done  # server stops after 25 s of input
kill -INT "$RTMP" 2>/dev/null; sleep 2; kill "$RTMP" 2>/dev/null
if [[ -s "$OUT/received.flv" ]]; then
  ffprobe -v error -show_entries stream=codec_type,codec_name,width,height,r_frame_rate:format=duration \
    -of compact "$OUT/received.flv" > "$OUT/received-probe.txt" 2>&1
  frames=$(ffprobe -v error -count_packets -select_streams v:0 -show_entries stream=nb_read_packets -of csv=p=0 "$OUT/received.flv" 2>/dev/null)
  grep -q "codec_type=audio" "$OUT/received-probe.txt" && aud=yes || aud=no
  if [[ "${frames:-0}" -gt 30 ]]; then pass "streamed to RTMP server: ${frames} video frames, audio=$aud"; else fail "stream reached the server but only ${frames:-0} video frames"; fi
else
  fail "nothing arrived at the RTMP server (rtmp-server.log, shots 06-08)"
fi
# End the stream (may ask to confirm).
grep -q "HopCast: streaming directly" "$OUT/app-first.log" && info "Go Live used the direct path (no PRISM registration)"
grep -q "\[ALERT\] Failed to start live" "$OUT/app-first.log" && fail "\"Failed to start live\" alert shown"
# The test server stops after 25 s; HopCast tries to reconnect, then End stops it.
grep -q "Reconnecting in" "$OUT/app-first.log" && pass "dropped connection: HopCast tried to reconnect"
click 1328 873; w 5; shot 09-end-live-1      # End
click 798 564; w 2; shot 10-end-live-2       # "Live Has Ended" -> OK
fdsnap after-live
for n in 0 1 2; do
  a=$(awk -v n=$n '$1==n{print $3}' "$OUT/fds-start.txt"); b=$(awk -v n=$n '$1==n{print $3}' "$OUT/fds-after-live.txt")
  [[ "$a" == "$b" ]] || fail "standard descriptor $n changed during the stream: '${a:-closed}' -> '${b:-closed}'"
done

# Record 10 s.
click 1460 873; w 3; shot 11-record-1
click 724 557; w 12; shot 12-recording       # "no video sources" -> Yes
click 1460 873; w 5; shot 13-record-stopped
click 798 527; w 2; shot 14-record-summary-closed   # "Recording Has Ended" -> OK
{ ls -la --time-style=full-iso "$START_MARK"; find "$HOME" -maxdepth 3 -type f -newermt "-15 minutes" -printf '%TY-%Tm-%Td %TT %s %p\n'; } > "$OUT/recording-search.txt" 2>&1
rec=$(find "$HOME" -type f \( -name '*.mkv' -o -name '*.mp4' -o -name '*.flv' -o -name '*.mov' \) -newer "$START_MARK" 2>/dev/null | grep -v received.flv | head -1)
if [[ -n "$rec" ]]; then
  d=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$rec" 2>/dev/null)
  info "recording written to ${rec#$HOME/} (${d:-?} s)"
  awk -v d="${d:-0}" 'BEGIN{exit !(d>=5)}' && pass "recording plays back (${d} s)" || fail "recording too short or unreadable (${d:-none})"
else
  fail "no recording file found under the home folder"
fi

quit_app first; code=$?
if [[ -s "$OUT/fd-guard.log" ]]; then
  # turn "lib.so(+0xOFF)[addr]" frames into function and source line
  while IFS= read -r l; do
    if [[ "$l" =~ ^(/[^\(]+)\(([^\)]*)\)\[ ]]; then
      lib="${BASH_REMATCH[1]}"; loc="${BASH_REMATCH[2]}"
      if [[ "$loc" == +0x* ]]; then
        echo "$(basename "$lib") $(addr2line -f -C -i -e "$lib" "${loc#+}" 2>/dev/null | paste -sd' ' | sed "s|$ROOT/||g")"
      else
        echo "$(basename "$lib") $loc"
      fi
    else
      echo "$l"
    fi
  done < "$OUT/fd-guard.log" > "$OUT/fd-guard-resolved.txt"
  fail "the app closed a standard descriptor (0-2) $(grep -c '^=== close' "$OUT/fd-guard.log") time(s): see fd-guard-resolved.txt"
fi
crashed "$OUT/app-first.log" && fail "first run crashed (backtrace in app-first.log)"
if (( code == 0 )); then pass "Ctrl+Q quits cleanly (exit 0)"; elif (( code == 124 )); then fail "app did not exit within 70 s of Ctrl+Q (hang on exit)"; else fail "app exited with status $code after Ctrl+Q"; fi
rm -f "$XDG_CONFIG_HOME/PRISMLiveStudio/.sentinel" 2>/dev/null

# ---------------------------------------------------------------- 3. second run on the same profile
start_app second
if wait_window; then pass "second start (same profile): main window shown"; else fail "second start: main window never appeared"; fi
w 6; shot 20-second-start
grep -qiE "tamper|key file is missing|KeyMissing" "$OUT/app-second.log" && fail "secure store reported a problem on second start" || pass "saved channel loaded without secure-store errors"
grep -q "Audit Server\|hopcast_rtmp.*load" "$OUT/app-second.log" && info "channel load logged"
quit_app second; code=$?
crashed "$OUT/app-second.log" && fail "second run crashed (backtrace in app-second.log)"
(( code == 0 )) && pass "second run quits cleanly" || fail "second run exit status $code"

# Did the first run's quit leave the crash marker (makes PRISM think it crashed)?
[[ -e "$XDG_CONFIG_HOME/PRISMLiveStudio/.sentinel" ]] && info "crash marker (.sentinel) present after clean quit"

# ---------------------------------------------------------------- 3b. repeated starts
# Intermittent startup hangs (e.g. fork/lock deadlocks) only show up over several starts.
RUNS="${AUDIT_RESTARTS:-8}"; hung=0; badexit=0
for i in $(seq 1 "$RUNS"); do
  rm -f "$XDG_CONFIG_HOME/PRISMLiveStudio/.sentinel"
  start_app "restart-$i"
  if ! wait_window; then
    hung=$((hung + 1)); shot "30-restart-$i-no-window"
    inf=$(pgrep -P "$APP" | head -1); [[ -n "$inf" ]] && kill -INT "$inf" 2>/dev/null; sleep 5
    pkill -KILL -f "$STAGE/bin/" 2>/dev/null; wait "$APP" 2>/dev/null; continue
  fi
  w 3
  quit_app "restart-$i"; code=$?
  (( code != 0 )) && badexit=$((badexit + 1))
  crashed "$OUT/app-restart-$i.log" && fail "start $i crashed (app-restart-$i.log)"
done
(( hung == 0 )) && pass "$RUNS starts in a row, all reached the main window" || fail "$hung of $RUNS starts never showed the main window (app-restart-*.log has stacks)"
(( badexit == 0 )) && pass "$RUNS quits in a row, all clean" || fail "$badexit of $RUNS quits did not exit cleanly"

kill "$COMP" "$XVFB" 2>/dev/null

# ---------------------------------------------------------------- 4. Wayland
if command -v weston >/dev/null; then
  weston --backend=headless-backend.so --socket=hopcast-wl --idle-time=0 --width=1600 --height=900 \
    > "$OUT/weston.log" 2>&1 & WESTON=$!
  sleep 3
  if kill -0 "$WESTON" 2>/dev/null; then
    ( export WAYLAND_DISPLAY=hopcast-wl QT_QPA_PLATFORM=wayland; unset DISPLAY; start_app wayland; echo "$APP" > "$OUT/.wl-pid"; wait "$APP"; echo $? > "$OUT/.wl-code" ) &
    WLSH=$!
    alive=1
    for _ in $(seq 1 $((45 * SLOW))); do sleep 1; [[ -f "$OUT/.wl-code" ]] && { alive=0; break; }; done
    if (( alive )); then
      pass "Wayland (native Qt platform): still running after $((45 * SLOW)) s"
      pkill -TERM -f "$STAGE/bin/" 2>/dev/null; sleep 5; pkill -KILL -f "$STAGE/bin/" 2>/dev/null
    else
      fail "Wayland: app exited early with status $(cat "$OUT/.wl-code") (app-wayland.log)"
    fi
    wait "$WLSH" 2>/dev/null
    crashed "$OUT/app-wayland.log" && fail "Wayland run crashed (backtrace in app-wayland.log)"
    grep -iE "qt.qpa|wayland|EGL|gl_|opengl" "$OUT/app-wayland.log" | grep -iE "error|fail|could not|unable" | head -20 > "$OUT/wayland-errors.txt"
    [[ -s "$OUT/wayland-errors.txt" ]] && info "Wayland graphics/platform errors: see wayland-errors.txt"
  else
    info "weston did not start; Wayland not tested (weston.log)"
  fi
  kill "$WESTON" 2>/dev/null
fi
pulseaudio --kill 2>/dev/null

# ---------------------------------------------------------------- 5. log review
# Was anything killed for running out of memory (sanitizer builds use a lot)?
sudo -n dmesg 2>/dev/null | grep -iE "out of memory|oom-kill|killed process" | tail -20 > "$OUT/oom.txt" || true
[[ -s "$OUT/oom.txt" ]] && fail "the kernel killed a process for lack of memory (oom.txt)"
free -m > "$OUT/memory.txt" 2>/dev/null || true
cat "$OUT"/app-*.log > "$APPLOG" 2>/dev/null
grep -E " (ERROR|WARN) |\[QT::(Warning|Critical)\]|runtime error:|AddressSanitizer|LeakSanitizer|QObject::connect|QLayout|Segmentation|terminate called" "$APPLOG" \
  | sed -E 's/^[0-9-]+ [0-9:.+]+ //; s/0x[0-9a-f]+/0x…/g; s/[0-9]{3,}/N/g' | sort | uniq -c | sort -rn > "$OUT/log-problems.txt"
info "$(wc -l < "$OUT/log-problems.txt") distinct warning/error lines in the app logs (log-problems.txt)"
echo; echo "=== SUMMARY ==="; cat "$SUMMARY"
grep -q "^FAIL" "$SUMMARY" && exit 1 || exit 0
