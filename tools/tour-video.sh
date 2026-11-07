#!/usr/bin/env bash
# Lamplight for PS5 - Plays the tour in the builder image and writes build/tour/lamplight-ps5.mp4,
# .webm, -poster.jpg, and docs/images/tour.gif for the README. Needs Docker and ffmpeg.
# Copyright (C) 2026 Hayden Griffin
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
out=build/tour
mkdir -p "$out"
docker run --rm -i -v "$root":/w -w /w -e HOST_CXX=clang++-18 -e HOST_CC=clang-18 lamplight-ps5-builder \
    bash tools/tour.sh |
    ffmpeg -hide_banner -loglevel error -y -f rawvideo -pixel_format rgba -video_size 1920x1080 -framerate 30 -i - \
        -vf "vflip,scale=1280:720:flags=lanczos,format=yuv420p" -an -c:v libx264 -preset slow -crf 26 \
        -movflags +faststart "$out/lamplight-ps5.mp4"
ffmpeg -hide_banner -loglevel error -y -i "$out/lamplight-ps5.mp4" -an -c:v libvpx-vp9 -crf 38 -b:v 0 -row-mt 1 \
    "$out/lamplight-ps5.webm"
ffmpeg -hide_banner -loglevel error -y -ss 0.8 -i "$out/lamplight-ps5.mp4" -frames:v 1 -q:v 3 "$out/lamplight-ps5-poster.jpg"
ffmpeg -hide_banner -loglevel error -y -i "$out/lamplight-ps5.mp4" -lavfi \
    "fps=10,scale=720:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=256:stats_mode=full[p];[b][p]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle" \
    docs/images/tour.gif
ls -la "$out" docs/images/tour.gif >&2
