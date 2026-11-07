#!/usr/bin/env bash
# Lamplight for PS5 - Builds the notification payload, dist/lamplight-notify.elf, with the payload SDK.
# Copyright (C) 2026 Hayden Griffin
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
bash tools/setup-native-dependencies.sh >/dev/null
sdk="$root/.deps/native/ps5-payload-sdk"
out="$root/build/payload"
mkdir -p "$out" "$root/dist"
flags=(-std=c++20 -O2 -Wall -Wextra -Werror -fno-exceptions -DJSON_NOEXCEPTION=1 -I"$root/src")
objects=()
bash tools/stubs.sh
for source in payload/notify.cpp payload/console.cpp payload/sfo.cpp payload/system.cpp src/afterglow/ma_socket.cpp src/lamplight/json.cpp src/lamplight/clock.cpp src/lamplight/config.cpp; do
    object="$out/$(basename "$source").o"
    "$sdk/bin/prospero-clang++" "${flags[@]}" -c "$source" -o "$object"
    objects+=("$object")
done
"$sdk/bin/prospero-clang++" -o "$root/dist/lamplight-notify.elf" "${objects[@]}" -lSceNotification \
    -lSceSystemService -lSceUserService
printf 'Payload: %s (%s bytes)\n' "dist/lamplight-notify.elf" "$(stat -c %s "$root/dist/lamplight-notify.elf")"
