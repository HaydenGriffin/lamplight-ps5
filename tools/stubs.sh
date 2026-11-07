#!/usr/bin/env bash
# Lamplight for PS5 - Builds the import stubs the payload SDK lacks (tools/stubs/*.c) into .deps/stubs.
# Copyright (C) 2026 Hayden Griffin
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=$root/.deps/stubs
mkdir -p "$out"
build() {
    local source=$1 soname=$2 library=$out/${2%.sprx}.so
    [[ $library -nt $source ]] && return
    clang-18 -target x86_64-unknown-linux -shared -nostdlib -fPIC -fuse-ld=lld -w \
        -Wl,-soname,"$soname" "$source" -o "$library"
}
build "$root/tools/stubs/audioin.c" libSceAudioIn.sprx
