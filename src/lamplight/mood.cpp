// Lamplight for PS5 - The resting clock's mood: the sky drawn live behind it, as the kiosks'
// Weather Mood and Celestial screensavers. Stars and the moon at night, clouds by day, rain and
// snow as the weather has them; everything slow and faint, so an OLED keeps nothing of it.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include <algorithm>
#include <cmath>

namespace lamplight
{
namespace
{
// A repeatable "random" number from 0 to 1 for the i-th thing of a kind.
float scatter(int i, int salt)
{
    auto x =
        static_cast<std::uint32_t>(i * 2654435761u + static_cast<std::uint32_t>(salt) * 40503u);
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    return static_cast<float>(x & 0xffffff) / static_cast<float>(0xffffff);
}
float wrap(float v, float span)
{
    return v - span * std::floor(v / span);
}
} // namespace

double moon_phase(std::int64_t utc_ms)
{
    // Days since the new moon of 6 January 2000, 18:14 UTC, in synodic months.
    const double days = static_cast<double>(utc_ms) / 86400000.0 - 10962.7597;
    const double month = 29.530588853;
    const double phase = std::fmod(days, month) / month;
    return phase < 0 ? phase + 1 : phase;
}
float moon_strength(bool late, float deep)
{
    return (late ? 0.3f : 0.6f) * (1 - 0.5f * deep);
}
void Application::draw_mood(Paint &p) const
{
    if (!config_.mood || ambient_ <= 0 || shown_picture(Pic::photo) || shown_picture(Pic::backdrop))
        return;
    const float a = ambient_;
    const float t = config_.reduced_motion ? 0.0f : time_;
    const bool night = after_dark();
    const auto weather = snapshot_.states.get(config_.weather).state;
    const bool rain = weather == "rainy" || weather == "pouring" || weather == "lightning-rainy";
    const bool snow = weather == "snowy" || weather == "snowy-rainy";
    const bool overcast = weather == "cloudy" || weather == "fog" || rain || snow;
    if (night && !overcast)
    {
        const bool late = late_at_night();
        const float deep = deep_rest();
        const float moon = moon_strength(late, deep);
        const float stars = (late ? 0.5f : 1.0f) * (1 - 0.5f * deep);
        for (int i = 0; i < 150; ++i)
        {
            const float twinkle = 0.55f + 0.45f * std::sin(t * 0.5f + static_cast<float>(i) * 1.7f);
            p.list.circle(scatter(i, 1) * 1920, scatter(i, 2) * 760, 1.2f + scatter(i, 3) * 1.6f,
                          Color::rgb(0xf4f2ee, a * 0.55f * twinkle * stars));
        }
        // The moon, in its phase: the lit part on the right while it waxes, the left as it wanes.
        const double phase = moon_phase(now());
        const float lit = static_cast<float>((1 - std::cos(phase * 6.283185307)) / 2);
        const float r = 58, cx = 1480 + 40 * std::sin(t / 700), cy = 236;
        p.list.glow({cx - r, cy - r, 2 * r, 2 * r}, r, 90,
                    Color::rgb(0xf4ecd8, a * 0.12f * lit * moon));
        // The dark part faintly (earthshine), then the lit part: from the bright limb round to the
        // terminator, an ellipse whose width follows the phase. Band by band, so nothing is drawn
        // twice along a terminator that is nearly straight.
        p.list.circle(cx, cy, r, Color::rgb(0xf4ecd8, a * 0.07f * moon));
        const float limb = phase < 0.5 ? 1.0f : -1.0f, terminator = limb * (1 - 2 * lit);
        constexpr int kBands = 48;
        const auto colour = Color::rgb(0xf4ecd8, a * 0.85f * moon);
        const auto half_at = [&](int band)
        {
            const float y = -r + 2 * r * static_cast<float>(band) / kBands;
            return std::sqrt(std::max(0.0f, r * r - y * y));
        };
        for (int i = 0; lit > 0.01f && i < kBands; ++i)
        {
            const float top = cy - r + 2 * r * static_cast<float>(i) / kBands;
            const float foot = cy - r + 2 * r * static_cast<float>(i + 1) / kBands;
            const float above = half_at(i), below = half_at(i + 1);
            // The first and last bands come to a point.
            if (i == 0)
            {
                const float tip[6] = {cx,  top, cx + terminator * below, foot, cx + limb * below,
                                      foot};
                p.list.polygon(tip, 3, colour);
                continue;
            }
            if (i == kBands - 1)
            {
                const float tip[6] = {
                    cx + limb * above, top, cx + terminator * above, top, cx, foot};
                p.list.polygon(tip, 3, colour);
                continue;
            }
            const float band[8] = {cx + limb * above,       top,  cx + terminator * above, top,
                                   cx + terminator * below, foot, cx + limb * below,       foot};
            p.list.polygon(band, 4, colour);
        }
    }
    // Clouds drift left to right, more of them when it's overcast.
    const int clouds = overcast ? 9 : night ? 0 : 5;
    for (int i = 0; i < clouds; ++i)
    {
        const float w = 380 + scatter(i, 4) * 320, h = 90 + scatter(i, 5) * 70;
        const float x = wrap(scatter(i, 6) * 2600 + t * (6 + scatter(i, 7) * 8), 2600) - 340;
        const float y = 70 + scatter(i, 8) * 380;
        p.list.shadow({x, y, w, h}, h / 2, 60, Color::rgb(0xf4f2ee, a * (night ? 0.04f : 0.08f)));
    }
    if (rain)
        for (int i = 0; i < 140; ++i)
        {
            const float x = scatter(i, 9) * 2000;
            const float y =
                wrap(scatter(i, 10) * 1200 + t * (700 + scatter(i, 11) * 300), 1200) - 60;
            p.list.line(x, y, x - 9, y + 34, 2, Color::rgb(0xa8c8ff, a * 0.22f));
        }
    if (snow)
        for (int i = 0; i < 110; ++i)
        {
            const float y = wrap(scatter(i, 12) * 1150 + t * (40 + scatter(i, 13) * 40), 1150) - 40;
            const float x = scatter(i, 14) * 1920 + 18 * std::sin(t * 0.7f + static_cast<float>(i));
            p.list.circle(x, y, 2.4f + scatter(i, 15) * 2, Color::rgb(0xffffff, a * 0.45f));
        }
}
} // namespace lamplight
