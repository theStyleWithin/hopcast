# Sourced by AppRun in the HopperCast *debug* AppImage.
# APPDIR is only set by the AppImage runtime, not when running an extracted AppRun.
_appdir="${APPDIR:-${this_dir:-$(dirname "$(readlink -f "$0")")}}"
export HOPCAST_DEBUG=1
_d="${XDG_DATA_HOME:-$HOME/.local/share}/hopcast/crash-logs"
mkdir -p "$_d"
# keep the 10 newest logs
ls -1t "$_d"/session-*.log 2>/dev/null | tail -n +10 | xargs -r rm -f
export HOPCAST_CRASH_LOG="$_d/session-$(date +%Y%m%d-%H%M%S).log"
export LD_PRELOAD="$_appdir/usr/lib/hopcast/libhopcast-crash.so${LD_PRELOAD:+:$LD_PRELOAD}"
export QT_LOGGING_RULES="*.debug=false"
exec > >(tee -a "$HOPCAST_CRASH_LOG") 2>&1
echo "HopperCast debug session $(date -Is) version $(cat "$_appdir/usr/share/hopcast/VERSION" 2>/dev/null)"
uname -a; echo "XDG_SESSION_TYPE=${XDG_SESSION_TYPE:-} DISPLAY=${DISPLAY:-} WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-}"
echo "Crash log: $HOPCAST_CRASH_LOG"
