# HopCast

> [!WARNING]
> **HopCast is in early development and is not ready for use. Please don't download, build or install it yet.**
> There are no releases. It has only been tested on automated build machines, not on a real desktop,
> and things will change or break without notice.

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
- **Your stream keys stay yours.** They are stored encrypted (AES-256-GCM), in a folder you choose.
- **Linux first.** Works on X11 and Wayland, and will be shipped as a single AppImage.

See **[SPECS.md](SPECS.md)** for the full description of what HopCast is and isn't.

## Status

| Area | State |
|---|---|
| Builds on Linux (engine + studio) | ✅ in automated builds (Ubuntu 24.04) |
| Studio interface, PRISM branding removed | ✅ |
| Streaming to an RTMP server, recording | ✅ in automated tests |
| Real-desktop testing (GPU encoders, webcams, screen capture) | ⏳ not yet |
| AppImage download | ⏳ not yet, no releases |

Developer notes, build steps and test details are in [`hopcast/README.md`](hopcast/README.md).
Screenshots of every screen are in [`hopcast/docs/ui-map/`](hopcast/docs/ui-map/README.md).

## License

HopCast is free software under the **GNU General Public License v2** ([`COPYING`](COPYING)).
Third-party components and their licenses are listed in [`LICENSES`](LICENSES).

- Based on PRISM Live Studio © NAVER Corp. and OBS Studio © the OBS Project contributors, both GPL-2.0.
  HopCast's own changes are GPL-2.0-or-later.
- "PRISM", "PRISM Live Studio", "NAVER" and "OBS" are trademarks of their owners. HopCast doesn't use them as its name or branding.
- HopCast comes with **no warranty**.

The original PRISM Live Studio README is kept for reference in
[`hopcast/docs/upstream/PRISM-README.md`](hopcast/docs/upstream/PRISM-README.md).
