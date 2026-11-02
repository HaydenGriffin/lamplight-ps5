// Lamplight for PS5 - The faces every page draws with.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ui/fonts.hpp"

namespace lamplight
{
struct Fonts
{
    hui::ui::FontRef text;      // Lamplight Text 400: Atkinson Hyperlegible Next, Lexend digits
    hui::ui::FontRef bold;      // Lamplight Text 600
    hui::ui::FontRef num;       // Lexend 600: standalone numerals, the clock
    hui::ui::FontRef num_light; // Lexend 400: the clock face's temperature
    hui::ui::FontRef icons;     // Material Design Icons (src/lamplight/icon_codes.hpp)
    hui::ui::Fonts kit;         // the kit's set, for its controller glyphs and lists
};
} // namespace lamplight
