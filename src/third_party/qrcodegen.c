/* Lamplight for PS5 - Builds the vendored QR Code generator (third_party/qrcodegen, MIT).
 * Copyright (C) 2026 Hayden Griffin
 * SPDX-License-Identifier: GPL-3.0-or-later
 * The console's C library has no assert, so its checks are compiled out. */
#define NDEBUG 1
#include "../../third_party/qrcodegen/qrcodegen.c"
