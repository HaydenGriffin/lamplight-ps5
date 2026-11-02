#!/usr/bin/env python3
# Lamplight for PS5 - Makes the source fonts the baker reads (needs fontTools, brotli and skia-pathops).
# Copyright (C) 2026 Hayden Griffin
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run on a machine with fontTools and brotli (not the Docker builder; pip install fonttools brotli skia-pathops), then `make fonts`.

  python3 tools/make-fonts.py <lamplight checkout>

- third_party/fonts/lexend_regular.ttf: Lexend at weight 400, from Lamplight's variable Lexend, so running text at
  400 has Lexend digits too (The Lexend Digits Rule).
- third_party/fonts/lamplight_icons.ttf: the Material Design Icons Lamplight uses, from @mdi/js's path data, one
  per private-use code point from U+E000 in ICONS order, each filling its 1200-unit em (ascent 1200, descent 0).
- src/lamplight/icon_codes.hpp: the names and code points, sorted by name for lookup.
"""
from pathlib import Path
import re
import sys

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.svgLib.path import parse_path
from fontTools.ttLib import TTFont
from fontTools.ttLib.removeOverlaps import removeOverlaps
from fontTools.varLib.instancer import instantiateVariableFont

ICONS = """
alarm alert-circle-outline arrow-left bell-outline brain briefcase-outline cake-variant-outline calendar-blank-outline
calendar-outline cart-outline check chevron-down chevron-left chevron-right chevron-up clock-outline close
cloud-alert-outline cog-outline comment-question-outline comment-text-outline home-heart home-outline information-outline
keyboard-outline key-variant lan-connect lan-disconnect lightbulb-group lightbulb-on-outline map-marker-outline
message-text-outline microphone-question music pin pin-outline play power refresh school-outline sun-wireless-outline
television thermometer timer-outline timer-pause-outline timer-sand trash-can-outline tumble-dryer undo washing-machine
dishwasher stove water-percent weather-cloudy weather-fog weather-hail weather-lightning weather-lightning-rainy
weather-night weather-night-partly-cloudy weather-partly-cloudy weather-pouring weather-rainy weather-snowy
weather-snowy-rainy weather-sunny weather-sunset-down weather-sunset-up weather-windy weather-windy-variant wifi
cellphone qrcode account-group heart-outline airplane soccer star-outline eye-off-outline robot-outline
sofa-outline ceiling-light floor-lamp lamp lightbulb-outline lightbulb-off-outline movie-open-outline
gamepad-variant-outline power-sleep pause skip-next speaker radiator fireplace bell-ring-outline door-open
fan blinds television-play music-note-off
""".split()
UPM = 1200
SCALE = UPM / 24


def camel(name):
    return "mdi" + "".join(part[:1].upper() + part[1:] for part in name.split("-"))


def mdi_paths(lamplight):
    source = (lamplight / "node_modules/@mdi/js/mdi.js").read_text()
    return dict(re.findall(r'export var (mdi\w+) = "([^"]+)"', source))


def icon_font(paths, out):
    order = [".notdef"] + [f"icon{i:03d}" for i in range(len(ICONS))]
    glyphs, metrics, cmap = {}, {}, {}
    glyphs[".notdef"] = TTGlyphPen(None).glyph()
    metrics[".notdef"] = (UPM, 0)
    for i, name in enumerate(ICONS):
        data = paths.get(camel(name))
        if data is None:
            sys.exit(f"@mdi/js has no {name}")
        pen = TTGlyphPen(None)
        # SVG's y runs down from the top of a 24-unit box; the font's runs up from the baseline.
        parse_path(data, TransformPen(Cu2QuPen(pen, 0.5, reverse_direction=True), (SCALE, 0, 0, -SCALE, 0, UPM)))
        glyph_name = order[i + 1]
        glyphs[glyph_name] = pen.glyph()
        metrics[glyph_name] = (UPM, 0)
        cmap[0xE000 + i] = glyph_name
    fb = FontBuilder(UPM, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=UPM, descent=0)
    fb.setupNameTable({"familyName": "Lamplight Icons", "styleName": "Regular"})
    fb.setupOS2(sTypoAscender=UPM, sTypoDescender=0, usWinAscent=UPM, usWinDescent=0)
    fb.setupPost()
    fb.save(out)


def header(out):
    rows = sorted((name, 0xE000 + i) for i, name in enumerate(ICONS))
    body = "\n".join(f'    {{"{name}", 0x{code:04X}}},' for name, code in rows)
    out.write_text(f"""// Lamplight for PS5 - Icon names and code points in lamplight-icons.huifont (tools/make-fonts.py).
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace lamplight
{{
struct IconCode
{{
    const char *name;
    std::uint32_t code;
}};
// Sorted by name. Material Design Icons (Apache-2.0), drawn from @mdi/js.
inline constexpr IconCode kIconCodes[] = {{
{body}
}};
}} // namespace lamplight
""")


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: tools/make-fonts.py <lamplight checkout with node_modules>")
    lamplight = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[1]
    lexend = TTFont(lamplight / "dist/lexend-latin.woff2")
    lexend.flavor = None
    instantiateVariableFont(lexend, {"wght": 400}, inplace=True)
    lexend.save(root / "third_party/fonts/lexend_regular.ttf")
    # stb_truetype's distance fields mark overlapping contours (Lexend's 1) with stray lines at
    # display sizes; merged outlines draw clean.
    for name in ("lexend_regular", "lexend_semibold", "atkinson_regular", "atkinson_semibold"):
        path = root / "third_party/fonts" / f"{name}.ttf"
        font = TTFont(path)
        removeOverlaps(font)
        font.save(path)
    icon_font(mdi_paths(lamplight), root / "third_party/fonts/lamplight_icons.ttf")
    header(root / "src/lamplight/icon_codes.hpp")
    print(f"{len(ICONS)} icons, Lexend 400")


if __name__ == "__main__":
    main()
