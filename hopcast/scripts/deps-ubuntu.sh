#!/usr/bin/env bash
# Build dependencies for the engine on Ubuntu 24.04+ (FFmpeg >= 6.1 is required,
# so 22.04 is too old). Mirrors the engine's own .github/scripts/utils.zsh/setup_ubuntu,
# minus CEF (browser source is a later phase).
set -euo pipefail
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build pkg-config git ccache file xvfb xauth \
  libglib2.0-dev libcurl4-openssl-dev \
  libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev libavutil-dev \
  libswresample-dev libswscale-dev \
  libjansson-dev libx11-xcb-dev libgles2-mesa-dev libwayland-dev \
  libpipewire-0.3-dev libpulse-dev libx264-dev libmbedtls-dev libgl1-mesa-dev \
  uthash-dev libluajit-5.1-dev python3-dev \
  libx11-dev libxcb-randr0-dev libxcb-shm0-dev libxcb-xinerama0-dev \
  libxcb-composite0-dev libxinerama-dev libxcb1-dev libxcb-xfixes0-dev \
  swig libcmocka-dev libxss-dev libglvnd-dev \
  libxkbcommon-dev libxcomposite-dev libxdamage-dev \
  libasound2-dev libfontconfig-dev libfreetype6-dev libjack-jackd2-dev \
  libsndio-dev libspeexdsp-dev libudev-dev libv4l-dev libva-dev \
  libpci-dev libdrm-dev \
  nlohmann-json3-dev libwebsocketpp-dev libasio-dev libqrcodegencpp-dev \
  libffmpeg-nvenc-dev librist-dev libsrt-openssl-dev \
  qt6-base-dev libqt6svg6-dev qt6-base-private-dev \
  libvpl-dev libvpl2
