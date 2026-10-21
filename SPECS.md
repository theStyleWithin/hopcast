# HopCast: Specification

**Status:** in development, no releases · **Version:** 0.1.0 · **License:** GPL-2.0

## 1. What HopCast is

HopCast is a desktop app for live streaming and recording on Linux. You build scenes from sources such
as cameras, screen capture, images, media and text. You can stream them live to a server of your choice
and record them to disk at the same time.

It is a modified version of the GPL-licensed source code of **PRISM Live Studio** (NAVER Corp.). That code
is itself built on **OBS Studio**. HopCast keeps PRISM's studio interface, which many people find friendlier
than OBS's, and makes four changes:

1. it runs natively on Linux;
2. it works fully offline, without accounts or company servers;
3. it treats every streaming destination equally;
4. it carries no PRISM or NAVER branding.

## 2. Goals

- **A friendly studio on Linux.** Scenes, sources, audio mixer, studio mode, dual (horizontal and vertical) output, recording, virtual camera.
- **Private by default.**
  - No sign-in and no telemetry.
  - No network traffic except the stream you start.
- **Neutral.** You enter your own server address and stream key. HopCast doesn't recommend, rank or advertise any platform.
- **Safe with secrets.** Stream keys are encrypted at rest and never written to logs.
- **Easy to install.** One AppImage that runs on current distributions (glibc 2.39 or newer: CachyOS, Arch, Fedora 40+, Ubuntu 24.04+).
- **Open.** GPL-2.0, with full source for every release.

## 3. Non-goals

- Accounts, cloud sync or anything that needs a HopCast or NAVER server.
- Built-in platform integrations (sign-in with Twitch/YouTube/Facebook, platform chat, viewer counts).
- Windows or macOS builds. Use PRISM Live Studio or OBS Studio there.
- Re-adding PRISM's online extras (PRISM Lens, sticker and template stores, notices, update checks).

## 4. Features

### Available in the current code

| Feature | Notes |
|---|---|
| Studio interface | PRISM's layout: channel bar, preview, scenes, sources, audio mixer, sidebar tools |
| Streaming | Custom RTMP, SRT or RIST; one destination per output (horizontal, plus vertical with dual output); reconnects automatically |
| Recording | MKV/MP4 to a folder of your choice; works before, during and after streaming |
| Sources | Video capture (V4L2), screen capture (PipeWire/X11), audio (PulseAudio/PipeWire/ALSA), images, media files, text, color |
| Encoders | x264 (software). NVENC and VAAPI need real-desktop testing |
| Stream-key storage | Kept inside HopCast's own storage, encrypted; no prompt and no folder to manage |
| Display servers | X11 and Wayland (native Qt) |

### Removed from PRISM

- Account sign-in, and the nine platform sign-in plugins.
- NAVER services: settings sync, notices, update checks, the live-status channel, the broadcast relay.
- Platform buttons and lists (Twitch, YouTube, Facebook, SOOP, NAVER TV, BAND, CHZZK, …).
- PRISM Lens, stickers, scene templates, the platform chat panel, the output-setting guide, help links, logos.

### Planned

- AppImage release and real-desktop testing.
- Remaining internal renaming (config folder, program name, leftover images).
- PRISM's local source plugins on Linux (timer, text templates, background music, effects).
- Streaming to several servers at once without a relay (a single encode sent to multiple outputs).

## 5. How it works

```
HopCast app (Qt 6)            PRISM's studio UI, Linux-ported, with NAVER services removed
  ├─ hopcast/securestore      AES-256-GCM storage for stream keys (OpenSSL)
  └─ engine (libobs 31)       NAVER's OBS fork: video/audio pipeline, encoders, outputs, Linux plugins
```

- The source is organized as upstream PRISM (`src/prism-live-studio`), the pinned engine (`src/obs-studio`, with
  HopCast patches in `hopcast/engine-patches/`) and HopCast's own tooling in `hopcast/`.
- Changes are switched on by build options: `HOPCAST_NO_ACCOUNT` and `HOPCAST_NO_NAVER` are on by default.
- A network guard refuses any request to NAVER/PRISM domains, and a link guard stops the app from opening their web pages.

## 6. Data and privacy

| What | Where | Protection |
|---|---|---|
| Stream keys and channels | `hopcast-rtmp-channels.enc` in HopCast's own folder (`~/.config/PRISMLiveStudio/user`) | AES-256-GCM; key file is owner-only (0600) |
| Encryption key | `~/.local/share/hopcast/secure-store.key` | owner-only; kept apart from the data |
| Crash-recovery state while live | `streaming_state.json.enc` | AES-256-GCM |
| Settings, scenes, logs | `~/.config/PRISMLiveStudio/` (to be renamed) | stream keys are masked in logs |

Limit: anyone or any program that can read your whole home folder as you can read both the key and the data.
An optional master password may be added later.

## 7. Quality checks

Every change is built automatically. The `hopcast-audit` workflow then runs the app with a sound server and a
real RTMP server. It checks streaming, reconnecting, recording, encrypted storage, clean exit and repeated
starts, X11 and Wayland, and runs static analysis. Details: [`hopcast/README.md`](hopcast/README.md).

## 8. Legal

- GPL-2.0. Source is published with every release, notices are kept, and changes are marked.
- Not affiliated with NAVER Corp. or the OBS Project. Their trademarks are used only to credit the original works.
- No warranty.
