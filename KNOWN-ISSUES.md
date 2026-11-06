# Known issues and open observations

Things that have been seen but not explained yet. Nothing here has a fix in progress unless it says so.

## Stream chat (Chat dock and chat overlay) removed: it crashed HopCast

- **Seen (v0.1.17 to v0.1.20, Linux AppImage on KDE Plasma with an NVIDIA RTX 2060):** opening View > Docks > Chat, or the
  tray menu's chat overlay, closed HopCast at once. It happened every time the chat's browser engine (Qt WebEngine) started.
  Drawing the chat with the processor instead of the graphics card (v0.1.20.1 test build) did not help.
- **Not known:** the exact cause. It works in the automated builds (software graphics), so it is something about the real
  graphics / window-system setup. No crash log from the affected computer has been collected.
- **Decision (the developer):** the chat feature is removed from the app for now (dock, overlay, Settings > General > Chat,
  the tray items, the browser engine library). Nothing else in HopCast used it.
- **To bring it back later:** the last code is in v0.1.20 (the `HopCastChat*` files and `hopcast/tests/chat-panel`). Start from
  a crash report from an affected computer, and keep the browser engine out of HopCast's own process.

## Wi-Fi dropped during the first multi-stream test (Steam Deck, Linux)

- **Seen:** after the first test streaming to four destinations at once, the Wi-Fi connection kept cycling
  "activated / disconnected / activated". It stopped only after HopCast was closed. The connection was fine
  when checked later.
- **Not known:** whether this comes from HopCast, from the upload load of four simultaneous streams, or from
  the Deck's Wi-Fi adapter. It hasn't been reproduced yet.
- **Plan (decided by the developer):** run many streaming tests to single platforms, one at a time, to see
  whether it can happen without multi-streaming and how stable each one is. No patch is attempted before that.
- **If it happens to you:** please report the version, how many destinations you were sending to, the bitrate,
  and the output of `sudo dmesg | tail -50` taken right after.
