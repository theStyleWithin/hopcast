#!/usr/bin/env bash
# Build dependencies for the engine on Arch / CachyOS / SteamOS-dev.
set -euo pipefail
sudo pacman -S --needed --noconfirm \
  base-devel cmake ninja git ccache pkgconf libsecret \
  ffmpeg x264 jansson curl mbedtls simde \
  libglvnd mesa wayland libxkbcommon \
  pipewire libpulse alsa-lib jack2 speexdsp \
  libx11 libxcb libxcomposite libxinerama libxss libxdamage \
  fontconfig freetype2 v4l-utils libva libdrm pciutils systemd-libs \
  luajit python swig cmocka uthash \
  nlohmann-json librist srt \
  qt6-base qt6-svg qt6-wayland \
  libvpl ffnvcodec-headers
# Package names follow Arch's own obs-studio PKGBUILD; not yet run on a real
# CachyOS box - report any "target not found" and we'll fix the list.
# websocketpp/asio/qrcodegen are only needed once ENABLE_WEBSOCKET is turned on.
