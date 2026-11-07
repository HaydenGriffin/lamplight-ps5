#!/usr/bin/env python3
# Lamplight for PS5 - Draws the site's picture collages and the repository's social preview from the preview renders (Pillow).
# Copyright (C) 2026 Hayden Griffin
# SPDX-License-Identifier: GPL-3.0-or-later
"""python3 tools/site-images.py  (after `make preview`). Writes build/site/*.webp and docs/images/social.png."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
PREVIEW = ROOT / "build/preview"
FONTS = ROOT / "third_party/fonts"
SITE = ROOT / "build/site"
PAGE, TEXT, MUTED = (10, 12, 15), (244, 242, 238), (185, 180, 172)
W, H = 1920, 1080


def render(name, size):
    return Image.open(PREVIEW / f"{name}.png").convert("RGBA").resize(size, Image.LANCZOS)


def rounded(im, radius):
    scale = 4
    mask = Image.new("L", (im.width * scale, im.height * scale), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, mask.width - 1, mask.height - 1), radius * scale, fill=255)
    out = Image.new("RGBA", im.size)
    out.paste(im, (0, 0), mask.resize(im.size, Image.LANCZOS))
    return out


def font(name, size):
    return ImageFont.truetype(str(FONTS / name), size)


def collage(names, out):
    """Four screens, two by two, at one size, on the page's near-black."""
    margin, gap = 40, 24
    cell = (W - 2 * margin - gap) // 2
    size = (cell, cell * 9 // 16)
    top = (H - 2 * size[1] - gap) // 2
    canvas = Image.new("RGBA", (W, H), PAGE + (255,))
    for index, name in enumerate(names):
        x = margin + (index % 2) * (cell + gap)
        y = top + (index // 2) * (size[1] + gap)
        canvas.alpha_composite(rounded(render(name, size), 18), (x, y))
    canvas.convert("RGB").save(SITE / out, quality=86, method=6)
    print(SITE / out)


def social():
    """1280 by 640, the shape GitHub's social preview takes: the lockup beside the clock on a TV."""
    canvas = Image.new("RGBA", (1280, 640), (11, 12, 15, 255))
    glow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse((-340, -380, 620, 420), fill=(255, 186, 84, 38))
    canvas.alpha_composite(glow.filter(ImageFilter.GaussianBlur(110)))
    icon = Image.open(ROOT / "sce_sys/icon0.png").convert("RGBA").crop((136, 136, 376, 376)).resize((96, 96), Image.LANCZOS)
    disc = Image.new("L", (384, 384), 0)
    ImageDraw.Draw(disc).ellipse((4, 4, 380, 380), fill=255)
    icon.putalpha(disc.resize((96, 96), Image.LANCZOS))
    canvas.alpha_composite(icon, (64, 150))
    d = ImageDraw.Draw(canvas)
    d.text((176, 146), "Lamplight", font=font("atkinson_semibold.ttf", 72), fill=TEXT)
    d.text((178, 226), "for PS5", font=font("atkinson_regular.ttf", 32), fill=MUTED)
    d.multiline_text((64, 300), "Your Home Assistant on the TV,\ndriven with a DualSense.", font=font("atkinson_semibold.ttf", 36),
                     fill=TEXT, spacing=8)
    d.multiline_text((64, 402), "Rooms, sliders for every light, music,\nphotos and notifications over any game.",
                     font=font("atkinson_regular.ttf", 25), fill=MUTED, spacing=8)
    tv = (640, 130, 1240, 470)
    d.rounded_rectangle(tv, 20, fill=(22, 24, 29), outline=(46, 50, 57), width=3)
    canvas.alpha_composite(rounded(render("home", (tv[2] - tv[0] - 20, tv[3] - tv[1] - 20)), 10), (tv[0] + 10, tv[1] + 10))
    d.rounded_rectangle((880, 470, 1000, 480), 4, fill=(36, 39, 45))
    out = ROOT / "docs/images/social.png"
    canvas.convert("RGB").save(out, optimize=True)
    print(out)


def main():
    SITE.mkdir(parents=True, exist_ok=True)
    render("home", (W, H)).convert("RGB").save(SITE / "ps5-hero.webp", quality=88, method=6)
    collage(["room", "music", "agenda", "timers-undo"], "ps5-views.webp")
    collage(["sheet-light", "sheet-climate", "sheet-house", "sheet-speakers"], "ps5-controls.webp")
    collage(["slideshow", "home-mood-night", "doorbell", "talk-thinking"], "ps5-moments.webp")
    social()


if __name__ == "__main__":
    main()
