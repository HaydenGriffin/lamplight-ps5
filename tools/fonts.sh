#!/usr/bin/env bash
# Lamplight for PS5 - Bakes the SDF fonts in assets/fonts from third_party/fonts.
# Copyright (C) 2026 Hayden Griffin
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/host assets/fonts
${HOST_CXX:-clang++} -std=c++20 -O2 -w tools/font-baker/bake_font.cpp -o build/host/bake_font
fonts=third_party/fonts
# Lamplight Text: Atkinson Hyperlegible Next for words, Lexend for the ten digits.
build/host/bake_font $fonts/atkinson_regular.ttf assets/fonts/text-regular.huifont 56 8 2048 european digits=$fonts/lexend_regular.ttf
build/host/bake_font $fonts/atkinson_semibold.ttf assets/fonts/text-semibold.huifont 56 8 2048 european digits=$fonts/lexend_semibold.ttf
# Standalone numerals up to the clock face, baked large so the biggest stay crisp.
build/host/bake_font $fonts/lexend_semibold.ttf assets/fonts/lexend-semibold.huifont 128 16 2048 basic
build/host/bake_font $fonts/lexend_regular.ttf assets/fonts/lexend-regular.huifont 96 12 2048 basic
build/host/bake_font $fonts/lamplight_icons.ttf assets/fonts/icons.huifont 96 10 2048 range:E000-E0FF
