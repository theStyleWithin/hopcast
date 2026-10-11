# HopCast

A Linux port of [PRISM Live Studio](https://github.com/naver/prism-live-studio), with a portable **AppImage** as the target.

> **Name note.** "HOPCAST" is a registered US mark (Reg. 6425832, John I. Haas, Inc.) limited to podcasts about hops and beer,
> a different market from streaming software. Low conflict risk; get a quick professional clearance before any commercial use.
> (The earlier codename "Hopper" was dropped: DISH Network owns and enforces it for TV-streaming products.)

## Legal

- This is a fork of PRISM Live Studio © NAVER Corp., licensed **GPL-2.0** (see `/COPYING`, `/LICENSES`).
  Our changes are also GPL-2.0-or-later. Full source must ship with every binary.
- The engine is NAVER's fork of OBS Studio (`src/obs-studio`, GPL-2.0-or-later), pinned at `a357328` (libobs 31.0.3).
- Not affiliated with or endorsed by NAVER Corp. or the OBS Project. "PRISM", "PRISM Live Studio" and "OBS"
  are their owners' marks. We don't use them as our name and won't ship their logos.
- No NAVER backend services, telemetry, or credentials. Streaming-platform logins use our own OAuth apps.

## Layout

Our additions live under `hopcast/` and `.github/workflows/hopcast-*.yml` so upstream merges stay clean.

```
hopcast/scripts/deps-ubuntu.sh       build deps, Ubuntu 24.04+ (FFmpeg >= 6.1 required)
hopcast/scripts/deps-arch.sh         build deps, Arch / CachyOS
hopcast/scripts/build-engine.sh      Phase 0: build the pinned engine (no UI)
hopcast/scripts/smoke-test-engine.sh Phase 0: headless engine + plugin load test
hopcast/tests/engine_smoke.c
hopcast/scripts/build-app.sh         Phase 1: build PRISM's layer, stage engine+app into build/stage
hopcast/scripts/launch-test-app.sh   Phase 1: run the staged app under Xvfb (gdb backtrace + screenshot)
hopcast/cmake/prism-linux.cmake      Linux-wide CMake adjustments + install layout for PRISM's tree
hopcast/engine-patches/              patch series applied to the pinned engine submodule
```

## Phase 0 — build the engine

```bash
bash hopcast/scripts/deps-arch.sh        # or deps-ubuntu.sh
bash hopcast/scripts/build-engine.sh
bash hopcast/scripts/smoke-test-engine.sh
```

Expected output ends with `PASS`.

### Phase 0 status: ✅ passing in CI (2026-10-09)

Verified on Ubuntu 24.04 (GitHub Actions, Xvfb + Mesa software GL):

```
libobs 31.0.3 started
video: OpenGL renderer up
audio: 48 kHz stereo up
input types registered: 17
  ffmpeg_source / v4l2_input / pulse_input_capture / image_source / color_source_v3  ok
created source 'smoke-color' (320x180)
PASS
```

What it took (NAVER's engine had never been built on Linux):

| Fix | Where |
|---|---|
| Export `PLS_VERSION` & friends (engine hard-requires them) | `build-engine.sh`, `hopcast/VERSION` |
| No-op `legacy_check()` (removed by NAVER, still used by obs-websocket) | `hopcast/cmake/engine-compat.cmake` |
| Skip macOS-only Objective-C `libobs/pls/*.m` on Linux | `hopcast/cmake/engine-compat.cmake` |
| Warnings not fatal (NAVER prints `uint64_t` with `%llu`) | `build-engine.sh` |
| `libobs/pls`: missing includes, Linux values, "no PRISM Lens" stubs | `engine-patches/0001` |
| POSIX `os_process_pipe_set_userdata()` (Windows-only, used by obs-ffmpeg) | `engine-patches/0002` |
| NVENC: Windows `FALSE` macro and MSVC `__try/__except` | `engine-patches/0003` |

Engine features **off for now**: browser source (CEF), websocket, WebRTC, VLC, scripting,
AJA/DeckLink, and the **VST host** (NAVER rewrote it for Windows/macOS only).

## Phase 1 — PRISM's own interface on Linux

### Status: ✅ builds, links and launches in CI (2026-10-11)

![PRISM's login window running on Linux (Xvfb, CI)](docs/phase1-first-launch.png)

- All of PRISM's common-libs and the PRISMLiveStudio app compile and link on Ubuntu 24.04 with Qt 6.8.3.
- `build-app.sh` stages a runnable tree in `build/stage` (engine install + app on top):
  `bin/PRISMLiveStudio`, `lib/` (libobs, PRISM libs, RPATH `$ORIGIN/../lib`), `lib/obs-plugins/`,
  `lib/hopcast/plugins/` (PRISM plugins), `share/obs/`, `share/hopcast/data/prism-studio/`.
  This is also the AppDir layout for the AppImage.
- CI launch test: the app starts, stays up for 30 s and shows PRISM's login window.
  Network requests fail cleanly because NAVER's public source ships with no server URLs.

Startup fixes made to get here (all marked `HopCast` in the source):

| Problem | Fix |
|---|---|
| `bringWindowToTop`, `system_info` undefined at link | Linux implementation in `platform-x11.cpp`; build `system-info-posix.cpp` |
| Stack overflow at startup in `findFileInResources` | Skip `.`/`..` (upstream bug, triggered by an empty path); Linux uses the shared POSIX gpop defaults |
| No Linux data layout | Generic install rules + `share/hopcast` data paths (`GetDataFilePath`, locale.ini, plugin path) |
| Null `PLSBasic` in side-bar hide event during construction (X11 realizes docks early) | Guarded |

### No accounts

PRISM normally won't open its studio until you sign in with a PRISM account (Facebook, Google,
Twitch, NAVER, Apple, LINE or e-mail), and that account system only works with NAVER's servers.
HopCast removes it: the CMake option `HOPCAST_NO_ACCOUNT` (on by default) skips the login window,
drops the File › Logout item and hides Settings › General › Account. The app opens straight into
the studio:

![HopCast studio window, no sign-in (Xvfb, CI)](docs/phase1-studio-no-account.png)

(The black rectangles in CI screenshots are translucent pop-ups — the welcome/notice dialogs and
the chat panel — which need a compositor to draw; on a normal desktop they render normally.)

### Next in Phase 1

- Channel bar: hide the NAVER-only services (NAVER TV, BAND, Shopping Live, CHZZK, SOOP) and
  keep Custom RTMP, which streams to Twitch/YouTube/Kick/etc. with a stream key and no login.
  Optional Twitch/YouTube sign-in (chat, titles) later, with HopCast's own registered app IDs.
- Drop the welcome/notice pop-ups that fetch NAVER content.
- PRISM source plugins (timer, text template, chat, sticker, …) on Linux.
- Real-desktop test on CachyOS (X11 and Wayland).

## AppImage target

- Built in CI on a fixed Ubuntu 24.04 image → runs on glibc ≥ 2.39 (CachyOS, Arch, Fedora 40+, Ubuntu 24.04+).
- Engine is built relocatable + portable-config so it runs from inside the AppImage mount.
- Releases are tagged and never rebuilt, so a given AppImage version always stays the same.
