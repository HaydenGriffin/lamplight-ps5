// Lamplight for PS5 - Lamplight's tokens and components (DESIGN.md) at television scale.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gfx/draw_list.hpp"
#include "lamplight/fonts.hpp"
#include <string>

namespace lamplight
{
using hui::gfx::Color;
using hui::gfx::Rect;
// The One Token Rule: every colour on screen is one of these.
namespace tone
{
constexpr Color page = Color::rgb(0x0b0c0f);
constexpr Color panel = Color::rgb(0x16181d, 0.74f);
constexpr Color panel_solid = Color::rgb(0x17191e);
constexpr Color raised = Color::rgb(0xffffff, 0.08f);
constexpr Color raised2 = Color::rgb(0xffffff, 0.14f);
constexpr Color lead = Color::rgb(0xffffff, 0.24f);
constexpr Color line = Color::rgb(0xffffff, 0.07f);
constexpr Color text = Color::rgb(0xf4f2ee);
constexpr Color muted = Color::rgb(0xb9b4ac);
constexpr Color warm = Color::rgb(0xffc76b);
constexpr Color warm_ink = Color::rgb(0x2a1a04);
constexpr Color warm_soft = Color::rgb(0xffba54, 0.16f);
constexpr Color warm_glow = Color::rgb(0xffba54, 0.45f);
constexpr Color accent = Color::rgb(0x80d8ff);
constexpr Color accent_ink = Color::rgb(0x04222e);
constexpr Color ok = Color::rgb(0x7ee2a8);
constexpr Color warn = Color::rgb(0xff9d5c);
constexpr Color alert = Color::rgb(0xff8a80);
constexpr Color dimmer = Color::rgb(0x000000, 0.78f);
constexpr Color toast = Color::rgb(0x0c0d10, 0.94f);
constexpr Color error_ground = Color::rgb(0x5a1616);
constexpr Color error_line = Color::rgb(0xff8a80, 0.45f);
constexpr Color error_text = Color::rgb(0xffd7d2);
constexpr Color danger_ink = Color::rgb(0x2b0a07);
constexpr Color glass = Color::rgb(0x0c0e12, 0.58f);
constexpr Color sky_muted = Color::rgb(0xf4f2ee, 0.76f);
constexpr Color well = Color::rgb(0x000000, 0.2f);
constexpr Color white = Color::rgb(0xffffff);
constexpr Color clear = Color::rgb(0x000000, 0.0f);
} // namespace tone
// Sizes: the tablet's CSS pixels scaled for a 1920 x 1080 television seen from a sofa.
// The 16px floor becomes 28.
namespace size
{
constexpr float caption = 28, label = 30, button = 32, body = 34, title = 44, headline = 56;
constexpr float numeral = 44, numeral_large = 60;
constexpr float margin = 96, top = 64, content_top = 220, content_bottom = 940;
constexpr float tile_radius = 36, small_radius = 24, chip_radius = 28;
constexpr float target = 88; // the 56px control floor
constexpr float disc = 80, disc_small = 64, disc_big = 112;
} // namespace size

enum class DiscKind
{
    rest,  // raised neutral
    on,    // warm fill, warm ink, lamp halo (The Lamp Rule)
    mode,  // sky fill, sky ink, no halo (The Sky Rule)
    warn,  // outlined orange (The Outlined Warning Rule)
    ok,    // mint on an ~18% ground
    alert, // coral on an ~18% ground
    raised2,
};
enum class PillKind
{
    rest,    // raised (raised-2 inside a panel)
    primary, // warm with warm ink: only when it turns something on
    lead,    // lead-neutral with a warm icon
    danger,  // alert fill, danger ink: the second press of a two-press confirm
    selected // sky with sky ink
};

class Paint
{
  public:
    Paint(hui::gfx::DrawList &list, const Fonts &fonts, float time, bool reduced_motion)
        : list(list), fonts(fonts), time(time), reduced_motion(reduced_motion)
    {
    }
    hui::gfx::DrawList &list;
    const Fonts &fonts;
    float time;
    bool reduced_motion;

    // Words in Lamplight Text; fitted to max_width with an ellipsis when it is given.
    float text(const std::string &s, float x, float baseline, float size, Color colour,
               bool bold = false, hui::gfx::Align align = hui::gfx::Align::left,
               float max_width = 0) const;
    float measure(const std::string &s, float size, bool bold = false) const;
    // Wrapped text from the first baseline; returns the baseline after the last line.
    float paragraph(const std::string &s, float x, float baseline, float size, float width,
                    float line_height, Color colour, int max_lines, bool bold = false,
                    hui::gfx::Align align = hui::gfx::Align::left) const;
    int lines(const std::string &s, float size, float width, bool bold = false) const;
    // A standalone number in Lexend with tabular digits, so a countdown never jitters.
    float number(const std::string &s, float x, float baseline, float size, Color colour,
                 hui::gfx::Align align = hui::gfx::Align::left, bool light = false) const;
    float number_width(const std::string &s, float size, bool light = false) const;
    // An MDI icon ("mdi:wifi" or "wifi") in a size x size square centred on (cx, cy).
    void icon(const std::string &name, float cx, float cy, float size, Color colour) const;
    void disc(float cx, float cy, float diameter, const std::string &icon, DiscKind kind) const;
    void panel(const Rect &r, float radius = size::tile_radius, Color fill = tone::panel) const;
    // The lamp wash: warm light from the top-left corner of something that is on.
    void wash(const Rect &r, float radius, Color colour = tone::warm_glow) const;
    // The focus ring: sky, outside the thing with a gap (3px outline, 2px offset on a tablet).
    void focus(const Rect &r, float radius) const;
    void pill(const Rect &r, const std::string &icon, const std::string &label, PillKind kind,
              bool focused, bool disabled = false, bool inside = false) const;
    // A slider across r: a track filled up to the value and a thumb (DESIGN.md, Sliders). With an
    // end colour the whole track is a scale from fill to end, as warm white to cool white.
    void slider(const Rect &r, float fraction, bool focused, Color fill = tone::warm,
                Color end = tone::clear) const;
    // The 10px warm live dot, pulsing on a 1.6 s loop (still under reduce motion).
    void live_dot(float cx, float cy, float diameter = 18) const;
    // A line through text, for a cancelled event.
    void strike(float x, float baseline, float width, float size, Color colour) const;
    // Load states (DESIGN.md): the "Couldn't load" block with Retry, and the empty block.
    void failed(const Rect &r, const std::string &what, bool focused) const;
    void empty(const Rect &r, const std::string &icon, const std::string &title,
               const std::string &detail) const;
};
// The icon's code point as UTF-8, or "" for a name not in the font.
std::string icon_glyph(const std::string &name);
bool has_icon(const std::string &name);
} // namespace lamplight
