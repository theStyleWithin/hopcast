# HopCast

> [!WARNING]
> **HopCast is in early development.** Test builds are on the [Releases](https://github.com/theStyleWithin/hoppercast/releases)
> page for people who want to help test. It has only been tested on automated build machines so far, not on a
> real desktop, and things will change or break without notice. Don't rely on it for a stream that matters yet.

HopCast is a free, open-source live-streaming and recording studio for Linux.

It is built from the open-source code of [PRISM Live Studio](https://github.com/naver/prism-live-studio)
(GPL-2.0) and [OBS Studio](https://obsproject.com) (GPL-2.0). HopCast keeps PRISM's studio layout and
adds a Linux port. It removes accounts, online services, platform advertising and PRISM branding.

HopCast is an independent project. It is **not affiliated with, endorsed by or supported by NAVER Corp.,
PRISM Live Studio or the OBS Project**.

## What makes it different

- **No account.** It opens straight into the studio.
- **No company servers.** HopCast never contacts NAVER/PRISM services, and has no telemetry or update pings.
- **No platform favoritism.** It streams to any RTMP, SRT or RIST server you enter, with no built-in buttons for particular platforms.
- **Several servers at once.** Switch on more than one channel and Go Live sends to all of them. The picture is encoded once, with no relay.
- **Your stream keys stay yours.** They are stored encrypted (AES-256-GCM), inside HopCast's own storage on your computer, with no account and no folder to manage.
- **Your settings stay put.** HopCast snapshots its settings, profiles, scenes and channels in the background (and once before the first run of each new version), puts back any settings file that goes missing or corrupt, and has Tools > Back up settings now / Restore settings from a backup. Snapshots live in `~/.local/share/hopcast/settings-backups` (owner-only; they include the key file, since saved stream keys can't be read without it).
- **Connects on its own.** If the first connection fails and you haven't chosen a network setting, HopCast retries over IPv4 and through each network interface by itself, so there's no Bind to IP to hunt for.
- **Linux first.** Works on X11 and Wayland, and ships as a single AppImage.

See **[SPECS.md](SPECS.md)** for the full description of what HopCast is and isn't.

## Status

| Area | State |
|---|---|
| Builds on Linux (engine + studio) | ✅ in automated builds (Ubuntu 24.04) |
| Studio interface, PRISM branding removed | ✅ |
| Streaming to an RTMP server, recording | ✅ in automated tests |
| Real-desktop testing (GPU encoders, webcams, screen capture) | ⏳ not yet |
| AppImage download | 🧪 test builds (normal + debug) on the Releases page |
| AUR (`hopcast-bin`) | 🧪 PKGBUILD attached to each release |
| Flathub | ⏳ later, once real-desktop testing is done |

## Trying a test build

> [!CAUTION]
> **Install and use at your own risk.** Test builds are early, untested on real desktops and may crash, lose
> settings or recordings, or misbehave. HopCast comes with no warranty (see [License](#license)). Back up anything
> important and don't rely on it for a stream or recording that matters.

Download `HopCast-release-<version>-x86_64.AppImage` from [Releases](https://github.com/theStyleWithin/hoppercast/releases), then:

```sh
chmod +x HopCast-release-*.AppImage
./HopCast-release-*.AppImage
```

It needs glibc 2.39 or newer (Ubuntu 24.04+, Fedora 40+, Arch, CachyOS).

If it crashes or misbehaves, run `HopCast-debug-*.AppImage` instead. It writes a session log, and a
backtrace if it crashes, to `~/.local/share/hopcast/crash-logs/`. Stream keys are masked in the log, but
don't type anything sensitive into the debug build, and read a log before you share it.

Developer notes, build steps and test details are in [`hopcast/README.md`](hopcast/README.md).
Screenshots of every screen are in [`hopcast/docs/ui-map/`](hopcast/docs/ui-map/README.md).

## License

HopCast is free software under the **GNU General Public License v2** ([`COPYING`](COPYING)).
Third-party components and their licenses are listed in [`LICENSES`](LICENSES).

- Based on PRISM Live Studio © NAVER Corp. and OBS Studio © the OBS Project contributors, both GPL-2.0.
  HopCast's own changes are GPL-2.0-or-later.
- "PRISM", "PRISM Live Studio", "NAVER" and "OBS" are trademarks of their owners. HopCast doesn't use them as its name or branding.
- The HopCast icon is original artwork made for this project (a camera with a record dot), free to use under GPL-2.0-or-later like the code. HopCast ships no PRISM or NAVER logos.
- HopCast comes with **no warranty**.

The original PRISM Live Studio README is kept for reference in
[`hopcast/docs/upstream/PRISM-README.md`](hopcast/docs/upstream/PRISM-README.md).
