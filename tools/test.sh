#!/usr/bin/env bash
# Lamplight for PS5 - Host tests under AddressSanitizer and UBSan, then the client against a fake
# Home Assistant.
# Copyright (C) 2026 Hayden Griffin
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
source tools/ninja-build.sh
cxx=$(command -v "${HOST_CXX:-clang++}")
cc=$(command -v "${HOST_CC:-clang}")
ninja_begin "$root/build/tests/build.ninja"
sources=(src/lamplight/*.cpp src/afterglow/ma_socket.cpp src/afterglow/artwork_fetch.cpp src/third_party/stb_image.cpp src/core/input.cpp src/core/save_file.cpp src/platform/ps5/keyboard.cpp host/platform_host.cpp src/gfx/font.cpp src/gfx/draw_list.cpp src/gfx/triangulate.cpp src/ui/theme.cpp src/ui/widgets.cpp src/ui/pixel_font.cpp src/ui/glyphs.cpp src/ui/components/component.cpp src/ui/components/list.cpp tests/tests.cpp)
objects=()
for source in "${sources[@]}"; do
  object="$root/build/tests/${source//\//_}.o"
  warnings=(-Wall -Wextra -Werror -Wno-unused-parameter)
  [[ $source == src/lamplight/* || $source == tests/* ]] || warnings=(-w)
  ninja_inputs=("$root/$source" "$cxx")
  ninja_edge CXX "$object" "${compiler_cache[@]}" "$cxx" -std=c++20 -O1 -g "${warnings[@]}" -fsanitize=address,undefined -fno-omit-frame-pointer -DJSON_NOEXCEPTION=1 -I"$root/src" -MD -MF "$object.d" -c "$root/$source" -o "$object"
  objects+=("$object")
done
object="$root/build/tests/qrcodegen.o"
ninja_inputs=("$root/src/third_party/qrcodegen.c" "$cc")
ninja_edge CC "$object" "$cc" -std=c11 -O1 -g -w -fsanitize=address,undefined -MD -MF "$object.d" -c "$root/src/third_party/qrcodegen.c" -o "$object"
objects+=("$object")
ninja_inputs=("${objects[@]}")
ninja_edge LINK "$root/build/tests/lamplight-tests" "$cxx" "${objects[@]}" -fsanitize=address,undefined -pthread -o "$root/build/tests/lamplight-tests"
ninja_run ${NINJA_ARGS:-}
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 build/tests/lamplight-tests
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 python3 tests/integration.py
