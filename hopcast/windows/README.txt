HopCast for Windows (early build, portable)
===========================================

Double-click HopCast.exe. There is no installer: nothing is installed, no administrator rights are
needed, and deleting this folder removes the program. Your settings live in
%APPDATA%\PRISMLiveStudio (delete that folder too if you want everything gone).

Please read:

* The developer uses Linux and only tests the AppImage versions of HopCast. This Windows build is made
  and checked automatically, but it hasn't been run by hand on a real Windows PC. Try it in a virtual
  machine (VirtualBox, VMware, Hyper-V) first, and tell us what you find.
* Windows SmartScreen may warn about an unknown publisher: the program isn't code-signed. Compare the
  file's SHA-256 (PowerShell: Get-FileHash HopCast-windows-x64-*.zip) with SHA256SUMS on the release page.
* HopCast has no account, no telemetry, and never suggests where to stream: type in your own server
  address and stream key.
* Left out of the Windows build for now: the virtual camera, capture-card (AJA/DeckLink) support and
  the scripting tool.

HopCast is free software (GPL-2.0), built on PRISM Live Studio and OBS Studio. Source:
https://github.com/theStyleWithin/hoppercast
