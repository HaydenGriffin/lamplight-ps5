#!/usr/bin/env python3
# Lamplight for PS5 - Draws docs/images/hero.png from the preview renders and a console capture (Pillow).
# Copyright (C) 2026 Hayden Griffin
# SPDX-License-Identifier: GPL-3.0-or-later
"""python3 tools/hero.py  (after `make preview`; build/console-notification.jpg is a Remote Play frame of a notification)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
F = ROOT / "third_party/fonts"
W, H = 3200, 1640
PAGE, TEXT, MUTED, WARM = (11, 12, 15), (244, 242, 238), (185, 180, 172), (255, 199, 107)


def font(name, size):
    return ImageFont.truetype(str(F / name), size)


def rounded(im, radius):
    mask = Image.new("L", im.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, im.size[0] - 1, im.size[1] - 1), radius, fill=255)
    out = Image.new("RGBA", im.size)
    out.paste(im, (0, 0), mask)
    return out


def shadow(canvas, box, radius, spread=40, alpha=150):
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(layer).rounded_rectangle(box, radius, fill=(0, 0, 0, alpha))
    canvas.alpha_composite(layer.filter(ImageFilter.GaussianBlur(spread)))


def main():
    hero = Image.new("RGBA", (W, H), PAGE + (255,))
    # The page's warm evening light from the top left (the lamp wash, very faint).
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse((-700, -900, 1500, 900), fill=(255, 186, 84, 34))
    hero.alpha_composite(glow.filter(ImageFilter.GaussianBlur(220)))
    d = ImageDraw.Draw(hero)
    # The lockup: the icon's lit disc and the name.
    # The icon's lit disc alone (its square ground would show on the page's warm light).
    icon = Image.open(ROOT / "sce_sys/icon0.png").convert("RGBA").crop((136, 136, 376, 376)).resize((150, 150), Image.LANCZOS)
    disc = Image.new("L", (600, 600), 0)
    ImageDraw.Draw(disc).ellipse((6, 6, 594, 594), fill=255)
    icon.putalpha(disc.resize((150, 150), Image.LANCZOS))
    halo = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(halo).ellipse((165, 245, 315, 395), fill=(255, 186, 84, 140))
    hero.alpha_composite(halo.filter(ImageFilter.GaussianBlur(28)))
    hero.alpha_composite(icon, (165, 245))
    d.text((350, 248), "Lamplight", font=font("atkinson_semibold.ttf", 132), fill=TEXT)
    d.text((356, 392), "for PS5", font=font("atkinson_regular.ttf", 54), fill=MUTED)
    d.multiline_text((160, 520), "Your house's evening light,\non the TV.", font=font("atkinson_semibold.ttf", 76),
                     fill=TEXT, spacing=14)
    d.multiline_text((160, 720), "The clock, the weather, the agenda, timers and\nyour living room, from Home Assistant, with\n"
                     "the DualSense. Its notifications show over games.", font=font("atkinson_regular.ttf", 46),
                     fill=MUTED, spacing=16)
    # Two insets: the room page, and a notification as the console showed it.
    d.text((164, 1012), "Living room", font=font("atkinson_semibold.ttf", 40), fill=MUTED)
    room = Image.open(ROOT / "build/preview/room.png").convert("RGBA").resize((800, 450), Image.LANCZOS)
    shadow(hero, (160, 1070, 960, 1520), 28)
    hero.alpha_composite(rounded(room, 28), (160, 1070))
    d.text((1030, 1012), "Over any game", font=font("atkinson_semibold.ttf", 40), fill=MUTED)
    capture = Image.open(ROOT / "build/console-notification.jpg").convert("RGBA")
    note = capture.crop((828, 21, 1257, 93)).resize((900, 158), Image.LANCZOS)
    shadow(hero, (1030, 1070, 1930, 1228), 24)
    hero.alpha_composite(rounded(note, 24), (1030, 1070))
    d.text((1030, 1256), "A Home Assistant notification on the PS5,\ncaptured from the console.",
           font=font("atkinson_regular.ttf", 34), fill=MUTED, spacing=10)
    # The TV: the clock, at rest on the wall.
    d.text((2050, 210), "The clock", font=font("atkinson_semibold.ttf", 40), fill=MUTED)
    tv = (2050, 270, 3080, 870)
    shadow(hero, tv, 34, 50, 170)
    d.rounded_rectangle(tv, 34, fill=(22, 24, 29), outline=(46, 50, 57), width=4)
    screen = Image.open(ROOT / "build/preview/home.png").convert("RGBA").resize((tv[2] - tv[0] - 36, tv[3] - tv[1] - 36),
                                                                          Image.LANCZOS)
    hero.alpha_composite(rounded(screen, 16), (tv[0] + 18, tv[1] + 18))
    d.rounded_rectangle((2470, 870, 2660, 884), 6, fill=(36, 39, 45))
    d.text((2050, 930), "Weather", font=font("atkinson_semibold.ttf", 40), fill=MUTED)
    weather = Image.open(ROOT / "build/preview/weather.png").convert("RGBA").resize((1030, 579), Image.LANCZOS)
    shadow(hero, (2050, 990, 3080, 1569), 28)
    hero.alpha_composite(rounded(weather, 28), (2050, 990))
    out = ROOT / "docs/images/hero.png"
    hero.convert("RGB").save(out, optimize=True)
    print(out, hero.size)


if __name__ == "__main__":
    main()
