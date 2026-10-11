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

### No accounts, no NAVER services, no PRISM branding

PRISM normally won't open its studio until you sign in with a PRISM account, and its account,
update, notice, banner, resource and channel-sync features all run on NAVER's servers. HopCast has
no right to use those servers (and the public source has none of their addresses or keys), so two
CMake options, both on by default, remove them:

- `HOPCAST_NO_ACCOUNT` — no login window, no File › Logout, no Settings › General › Account.
- `HOPCAST_NO_NAVER`
  - **Hard network block:** `libhttp-client` refuses any request to a NAVER-operated host
    (naver.com/.net, navercorp.com, pstatic.net, prismlive.com, NAVER Cloud, band.us, …) and any
    unset endpoint, before a connection is made. The launch test reports blocked/leaked requests.
  - **No NAVER calls at all at startup:** PRISM token, app-init, gpop, update check, notices,
    thumbnails, banner and the resource CDN are skipped; bundled defaults are used.
  - **Custom RTMP channels are saved locally**, encrypted (see *Stream keys* below), instead of
    on NAVER's server.
  - **Channels:** NAVER TV, BAND, Shopping Live, CHZZK, NCB2B, Whale and NOW removed.
  - **Removed pop-ups:** welcome/banner window, PRISM Lens onboarding. Help menu keeps only Program Info.
  - **Name:** title bar text "HopCast" with no logo image; window and tray title "HopCast" with
    the desktop theme's generic camera icon.

![HopCast studio window (Xvfb, CI)](docs/phase1-studio-no-account.png)

(The black area on the right in CI screenshots is the translucent chat panel, which needs a
compositor to draw; on a normal desktop it renders normally.)

### Stream keys are encrypted

Everything that holds a stream key or a platform token is encrypted on disk by HopCast
(`hopcast/securestore`, tests in `hopcast/tests/secure_store_test.cpp`):

| File (under `~/.config/PRISMLiveStudio/`) | Contents |
|---|---|
| `user/hopcast-rtmp-channels.enc` | your Custom RTMP channels (server + stream key) |
| `Cache/channel_cache.dat.enc` | PRISM's channel cache (all channels, keys, tokens) |

- **AES-256-GCM.** The files can't be read without the key, and any change made outside HopCast
  is detected: the file is not loaded, it's set aside as `….unreadable-<time>`, and HopCast tells
  you to re-enter your keys. Only HopCast, when you change a key in the app, writes these files
  (atomically, mode 0600).
- **The key lives in your desktop keyring** (Secret Service: KWallet on Plasma, GNOME Keyring,
  KeePassXC…), protected by your login password, under "HopCast secure store key".
  If no keyring is running, or it refuses to store the key, HopCast uses an owner-only key file
  (`user/.hopcast-store.key`) instead and says so in its log. That still keeps the data safe from
  other users and from someone copying the data file alone, but not from anyone who can read
  your whole home folder.
- **Keyring locked or missing later?** HopCast won't overwrite data it can't read: your saved
  channels stay untouched, HopCast tells you, and saving is paused until the keyring is available.
- **No plaintext copies:** older plaintext files (`hopcast-rtmp-channels.json`, `channel_cache.dat`)
  are encrypted and then overwritten and deleted on first start. `service.json` no longer stores
  the key (it's re-applied from your channel at every Go Live), and PRISM's unmasked "KR" log
  lines — which printed keys and tokens — are dropped.
- **Limits, honestly:** while your keyring is unlocked, any program running as *you* can ask it
  for secrets (Linux keyrings have no per-app access control; KWallet may ask first). HopCast has
  to decrypt a key in memory to stream with it, and the key is sent to your streaming platform
  when you go live — that's how RTMP works. On SSDs, overwritten old plaintext files may still
  physically linger until the drive reuses those blocks.

### Next in Phase 1

- Twitch / YouTube / Facebook / SOOP buttons still use PRISM's sign-in plugins (not built on Linux
  yet, and they'd need HopCast's own registered app IDs). Until then, use **Custom RTMP** with a
  stream key — it works for Twitch, YouTube, Kick, etc.
- Remaining PRISM text in less visible places (dialog titles, translations, About box, the
  `PRISMLiveStudio` config folder and binary name) — full rebrand pass.
- PRISM source plugins (timer, text template, chat, sticker, …) on Linux.
- Real-desktop test on CachyOS (X11 and Wayland).

## AppImage target

- Built in CI on a fixed Ubuntu 24.04 image → runs on glibc ≥ 2.39 (CachyOS, Arch, Fedora 40+, Ubuntu 24.04+).
- Engine is built relocatable + portable-config so it runs from inside the AppImage mount.
- Releases are tagged and never rebuilt, so a given AppImage version always stays the same.
