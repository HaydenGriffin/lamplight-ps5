// Lamplight for PS5 - Lamplight's tokens and components (DESIGN.md) at television scale.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/paint.hpp"
#include "lamplight/icon_codes.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace lamplight
{
using hui::gfx::Align;
namespace
{
std::uint32_t icon_code(const std::string &name)
{
    const std::string key = name.starts_with("mdi:") ? name.substr(4) : name;
    const auto *first = std::begin(kIconCodes), *last = std::end(kIconCodes);
    const auto *it = std::lower_bound(first, last, key, [](const IconCode &a, const std::string &k)
                                      { return std::strcmp(a.name, k.c_str()) < 0; });
    return it != last && key == it->name ? it->code : 0;
}
std::size_t utf8_length(unsigned char lead)
{
    return lead < 0x80         ? 1
           : (lead >> 5) == 6  ? 2
           : (lead >> 4) == 14 ? 3
           : (lead >> 3) == 30 ? 4
                               : 1;
}
bool is_digit(const std::string &s, std::size_t at)
{
    return s[at] >= '0' && s[at] <= '9';
}
// A time's digits keep their width, so a running one doesn't jitter; a still number (12 degrees)
// sets naturally rather than with gaps.
bool is_time(const std::string &s)
{
    return s.find(':') != std::string::npos;
}
} // namespace

std::string icon_glyph(const std::string &name)
{
    const auto code = icon_code(name);
    if (!code)
        return "";
    std::string out;
    out += static_cast<char>(0xE0 | (code >> 12));
    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (code & 0x3F));
    return out;
}
bool has_icon(const std::string &name)
{
    return icon_code(name) != 0;
}
float Paint::text(const std::string &s, float x, float baseline, float sz, Color colour, bool bold,
                  Align align, float max_width) const
{
    const auto &f = bold ? fonts.bold : fonts.text;
    const std::string shown = max_width > 0 ? f.font->fit(s, sz, max_width) : s;
    return hui::ui::text(list, f, shown, x, baseline, sz, colour, align);
}
float Paint::measure(const std::string &s, float sz, bool bold) const
{
    return (bold ? fonts.bold : fonts.text).measure(s, sz);
}
float Paint::paragraph(const std::string &s, float x, float baseline, float sz, float width,
                       float line_height, Color colour, int max_lines, bool bold, Align align) const
{
    return hui::ui::paragraph(list, bold ? fonts.bold : fonts.text, s, x, baseline, sz, width,
                              line_height, colour, max_lines, align);
}
int Paint::lines(const std::string &s, float sz, float width, bool bold) const
{
    return static_cast<int>((bold ? fonts.bold : fonts.text).font->wrap(s, sz, width).size());
}
float Paint::number_width(const std::string &s, float sz, bool light) const
{
    const auto &f = light ? fonts.num_light : fonts.num;
    float cell = 0;
    for (char d = '0'; d <= '9'; ++d)
        cell = std::max(cell, f.measure(std::string(1, d), sz));
    float width = 0;
    for (std::size_t i = 0; i < s.size();)
    {
        const auto n = utf8_length(static_cast<unsigned char>(s[i]));
        width += is_digit(s, i) && is_time(s) ? cell : f.measure(s.substr(i, n), sz);
        i += n;
    }
    return width;
}
float Paint::number(const std::string &s, float x, float baseline, float sz, Color colour,
                    Align align, bool light) const
{
    const auto &f = light ? fonts.num_light : fonts.num;
    float cell = 0;
    for (char d = '0'; d <= '9'; ++d)
        cell = std::max(cell, f.measure(std::string(1, d), sz));
    const float width = number_width(s, sz, light);
    float pen = align == Align::left ? x : align == Align::center ? x - width / 2 : x - width;
    for (std::size_t i = 0; i < s.size();)
    {
        const auto n = utf8_length(static_cast<unsigned char>(s[i]));
        const auto part = s.substr(i, n);
        if (is_digit(s, i) && is_time(s))
        {
            hui::ui::text(list, f, part, pen + cell / 2, baseline, sz, colour, Align::center);
            pen += cell;
        }
        else
            pen += hui::ui::text(list, f, part, pen, baseline, sz, colour);
        i += n;
    }
    return width;
}
void Paint::icon(const std::string &name, float cx, float cy, float sz, Color colour) const
{
    auto glyph = icon_glyph(name);
    if (glyph.empty())
        glyph = icon_glyph("information-outline");
    // Each icon fills its em, which is the font's whole ascent: the baseline is the box's foot.
    hui::ui::text(list, fonts.icons, glyph, cx - sz / 2, cy + sz / 2, sz, colour);
}
void Paint::disc(float cx, float cy, float d, const std::string &name, DiscKind kind) const
{
    const float r = d / 2;
    const Rect box{cx - r, cy - r, d, d};
    Color fill = tone::raised, ink = tone::text;
    switch (kind)
    {
    case DiscKind::rest:
        break;
    case DiscKind::raised2:
        fill = tone::raised2;
        break;
    case DiscKind::on:
        // The lamp halo: 0 0 22px warm-glow, only on something that is on.
        list.glow(box, r, 22 * 1.6f, tone::warm_glow);
        fill = tone::warm;
        ink = tone::warm_ink;
        break;
    case DiscKind::mode:
        fill = tone::accent;
        ink = tone::accent_ink;
        break;
    case DiscKind::warn:
        fill = tone::clear;
        ink = tone::warn;
        list.ring(cx, cy, r, 3, tone::warn);
        break;
    case DiscKind::ok:
        fill = tone::ok.with_alpha(0.18f);
        ink = tone::ok;
        break;
    case DiscKind::alert:
        fill = tone::alert.with_alpha(0.18f);
        ink = tone::alert;
        break;
    }
    if (fill.a > 0)
        list.circle(cx, cy, r, fill);
    icon(name, cx, cy, d * 0.54f, ink);
}
void Paint::panel(const Rect &r, float radius, Color fill) const
{
    list.rounded_rect(r, radius, fill);
}
void Paint::wash(const Rect &r, float radius, Color colour) const
{
    // radial-gradient(120% 100% at 0% 0%, warm .30, transparent 62%): light spilling from a
    // point at the corner, inside the tile's rounded shape.
    list.corner_glow(r, radius, std::max(r.w, r.h) * 0.62f, colour.with_alpha(0.66f));
}
void Paint::focus(const Rect &r, float radius) const
{
    const float gap = 6, width = 5;
    list.bordered_rect(
        {r.x - gap - width, r.y - gap - width, r.w + 2 * (gap + width), r.h + 2 * (gap + width)},
        radius + gap + width, tone::clear, width, tone::accent);
}
void Paint::pill(const Rect &r, const std::string &name, const std::string &label, PillKind kind,
                 bool focused, bool disabled, bool inside) const
{
    Color fill = inside ? tone::raised2 : tone::raised, ink = tone::text, icon_colour = tone::text;
    if (kind == PillKind::primary)
    {
        fill = tone::warm;
        ink = icon_colour = tone::warm_ink;
    }
    else if (kind == PillKind::lead)
    {
        fill = tone::lead;
        icon_colour = tone::warm;
    }
    else if (kind == PillKind::danger)
    {
        fill = tone::alert;
        ink = icon_colour = tone::danger_ink;
    }
    else if (kind == PillKind::selected)
    {
        // Quiet, so the focus ring is the one loud thing: selected isn't where you are.
        fill = tone::accent.with_alpha(0.2f);
        ink = icon_colour = tone::accent;
    }
    if (disabled)
        list.push_opacity(0.45f);
    if (focused && kind == PillKind::rest)
        fill = inside ? tone::lead : tone::raised2;
    list.rounded_rect(r, r.h / 2, fill);
    const float icon_size = r.h * 0.42f;
    const float label_width = label.empty() ? 0 : measure(label, size::button, true);
    const float gap = label.empty() || name.empty() ? 0 : 14;
    const float content = (name.empty() ? 0 : icon_size) + gap + label_width;
    float x = r.cx() - content / 2;
    if (!name.empty())
    {
        icon(name, x + icon_size / 2, r.cy(), icon_size, icon_colour);
        x += icon_size + gap;
    }
    if (!label.empty())
        text(label, x, r.cy() + size::button * 0.36f, size::button, ink, true, Align::left,
             r.w - (x - r.x) - 24);
    if (disabled)
        list.pop_opacity();
    if (focused)
        focus(r, r.h / 2);
}
void Paint::slider(const Rect &r, float fraction, bool focused, Color fill, Color end) const
{
    constexpr float kTrack = 14;
    const Rect track{r.x, r.cy() - kTrack / 2, r.w, kTrack};
    const float x = r.x + r.w * std::clamp(fraction, 0.0f, 1.0f);
    if (end.a > 0)
        list.gradient_rect_h(track, kTrack / 2, fill, end);
    else
    {
        list.rounded_rect(track, kTrack / 2, tone::raised2);
        list.rounded_rect({track.x, track.y, x - track.x, track.h}, kTrack / 2, fill);
    }
    const float thumb = focused ? 26 : 20;
    list.shadow({x - thumb, r.cy() - thumb, 2 * thumb, 2 * thumb}, thumb, 14,
                Color::rgb(0x000000, 0.5f));
    list.circle(x, r.cy(), thumb, tone::text);
    if (focused)
        list.ring(x, r.cy(), thumb + 10, 5, tone::accent);
}
void Paint::live_dot(float cx, float cy, float d) const
{
    float alpha = 1;
    if (!reduced_motion)
        alpha = 0.55f + 0.45f * (0.5f + 0.5f * std::cos(time * 6.2832f / 1.6f));
    list.circle(cx, cy, d / 2, tone::warm.with_alpha(alpha));
}
void Paint::strike(float x, float baseline, float width, float sz, Color colour) const
{
    const float y = baseline - sz * 0.3f;
    list.line(x, y, x + width, y, 3, colour);
}
void Paint::failed(const Rect &r, const std::string &what, bool focused) const
{
    // A 64px outlined warning disc, "Couldn't load the forecast", a muted line and Retry.
    const float cx = r.cx();
    float y = r.cy() - 170;
    disc(cx, y + 56, 112, "cloud-alert-outline", DiscKind::warn);
    text("Couldn't load " + what, cx, y + 170, size::title, tone::text, true, Align::center,
         r.w - 64);
    text("Check the connection, then try again.", cx, y + 222, size::label, tone::muted, false,
         Align::center, r.w - 64);
    pill({cx - 120, y + 262, 240, size::target}, "refresh", "Retry", PillKind::rest, focused, false,
         true);
}
void Paint::empty(const Rect &r, const std::string &name, const std::string &title,
                  const std::string &detail) const
{
    const float cx = r.cx(), y = r.cy() - 110;
    disc(cx, y + 64, 128, name, DiscKind::raised2);
    text(title, cx, y + 190, size::title, tone::text, true, Align::center, r.w - 64);
    paragraph(detail, cx, y + 240, size::label, r.w - 80, 42, tone::muted, 3, false, Align::center);
}
} // namespace lamplight
