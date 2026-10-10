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
```

## Phase 0 — build the engine

```bash
bash hopcast/scripts/deps-arch.sh        # or deps-ubuntu.sh
bash hopcast/scripts/build-engine.sh
bash hopcast/scripts/smoke-test-engine.sh
```

Expected output ends with `PASS`.

## AppImage target

- Built in CI on a fixed Ubuntu 24.04 image → runs on glibc ≥ 2.39 (CachyOS, Arch, Fedora 40+, Ubuntu 24.04+).
- Engine is built relocatable + portable-config so it runs from inside the AppImage mount.
- Releases are tagged and never rebuilt, so a given AppImage version always stays the same.
