#!/usr/bin/env bash
# Lamplight for PS5 - Builds the tour (host/tour.cpp, Mesa, offscreen) and writes its raw frames to
# stdout. Runs in the builder image; tools/tour-video.sh turns the frames into videos.
# Copyright (C) 2026 Hayden Griffin
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
source tools/ninja-build.sh
cxx=$(command -v "${HOST_CXX:-clang++}")
cc=$(command -v "${HOST_CC:-clang}")
ninja_begin "$root/build/tour-build/build.ninja"
sources=(src/lamplight/*.cpp src/afterglow/ma_socket.cpp src/afterglow/artwork_fetch.cpp src/core/input.cpp src/core/save_file.cpp src/platform/ps5/keyboard.cpp host/platform_host.cpp host/tour.cpp src/third_party/stb_image.cpp src/gfx/*.cpp src/ui/*.cpp src/ui/components/*.cpp)
objects=()
for source in "${sources[@]}"; do
  object="$root/build/tour-build/${source//\//_}.o"
  ninja_inputs=("$root/$source" "$cxx")
  ninja_edge CXX "$object" "${compiler_cache[@]}" "$cxx" -std=c++20 -O2 -Wall -Wextra -DGL_GLEXT_PROTOTYPES=1 -DJSON_NOEXCEPTION=1 -I"$root/src" -MD -MF "$object.d" -c "$root/$source" -o "$object"
  objects+=("$object")
done
object="$root/build/tour-build/qrcodegen.o"
ninja_inputs=("$root/src/third_party/qrcodegen.c" "$cc")
ninja_edge CC "$object" "$cc" -std=c11 -O2 -w -MD -MF "$object.d" -c "$root/src/third_party/qrcodegen.c" -o "$object"
objects+=("$object")
ninja_inputs=("${objects[@]}")
ninja_edge LINK "$root/build/tour-build/lamplight-tour" "$cxx" "${objects[@]}" -lEGL -lGL -pthread -o "$root/build/tour-build/lamplight-tour"
ninja_run ${NINJA_ARGS:-} >&2
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe build/tour-build/lamplight-tour
