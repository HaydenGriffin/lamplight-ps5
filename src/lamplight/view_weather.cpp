// Lamplight for PS5 - Weather: now, the next 12 hours as a chart, and the next five days.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include <algorithm>
#include <cmath>

namespace lamplight
{
using hui::gfx::Align;
namespace
{
std::string two_digit_hour(std::int64_t at, const Zone &zone)
{
    return two(local(at, zone).hour);
}
// Line colour by sky, as the card's --chart-line.
Color chart_line(Sky sky)
{
    return sky == Sky::rain    ? Color::rgb(0x9fd3ff)
           : sky == Sky::night ? Color::rgb(0xb8c4ff)
                               : tone::warm;
}
} // namespace

void Application::draw_weather(Paint &p) const
{
    const auto &w = snapshot_.states.get(config_.weather);
    const auto &a = w.attributes;
    const auto state = forecast_state();
    draw_header(p, "Weather",
                state == "failed" ? "Forecast unavailable \xC2\xB7 checked " +
                                        hhmm(forecast_failed_at_, clock_.zone)
                                  : "",
                tone::warn);
    // Sunrise and sunset, beside the header's clock.
    const auto &sun = snapshot_.states.get("sun.sun");
    if (sun.available())
    {
        float x = 1440;
        for (const auto &[icon, key] : {std::pair{"weather-sunset-down", "next_setting"},
                                        std::pair{"weather-sunset-up", "next_rising"}})
        {
            const auto at = parse_iso_ms(text(sun.attributes, key), clock_.zone);
            const auto label = at == kNoTime ? std::string("\xE2\x80\x93") : hhmm(at, clock_.zone);
            const float width = p.number_width(label, size::button);
            p.number(label, x, 118, size::button, tone::sky_muted, Align::right);
            p.icon(icon, x - width - 30, 106, 40, tone::warm);
            x -= width + 90;
        }
    }
    if (config_.weather.empty() || (!w.available() && !forecast_.at))
    {
        p.empty({size::margin, size::content_top, 1728, 700}, "weather-partly-cloudy",
                config_.weather.empty() ? "No weather chosen" : "Weather not available",
                config_.weather.empty() ? "Choose a weather entity in Setup (Options)."
                                        : "Home Assistant doesn't have this weather entity now.");
        return;
    }
    // Now: the one big number on the page; while it can't be read, the last forecast stays.
    if (!w.available())
    {
        p.text("Not available now", size::margin, 420, size::title, tone::text, true);
        p.paragraph("Showing the forecast from " + hhmm(forecast_.at, clock_.zone) + ".",
                    size::margin, 476, size::label, 520, 42, tone::sky_muted, 3);
    }
    else
    {
        // Now: the one big number on the page.
        const float left = size::margin;
        p.icon(condition_icon(w.state), left + 72, 352, 144, tone::warm);
        p.number(degrees(number(field(a, "temperature"))), left + 168, 420, 196, tone::text);
        p.text(condition_label(w.state), left, 506, size::title + 4, tone::text, true, Align::left,
               520);
        const Day *today = nullptr;
        for (const auto &d : forecast_.days)
            if (day_start(d.at, clock_.zone) == day_start(now(), clock_.zone))
            {
                today = &d;
                break;
            }
        if (today)
            p.text("High " + degrees(today->high) + " \xC2\xB7 Low " + degrees(today->low), left,
                   556, size::body, tone::sky_muted);
        const auto unit = text(a, "wind_speed_unit", "km/h");
        struct Stat
        {
            const char *icon;
            std::string value, caption;
        };
        std::vector<Stat> stats{
            {"weather-windy", std::to_string(rounded(number(field(a, "wind_speed")))),
             unit + " " + compass(number(field(a, "wind_bearing")))},
            {"water-percent", std::to_string(rounded(number(field(a, "humidity")))) + "%",
             "humidity"}};
        if (has_number(field(a, "uv_index")))
            stats.push_back({"sun-wireless-outline",
                             std::to_string(rounded(number(field(a, "uv_index")))), "UV"});
        float sx = left;
        for (const auto &s : stats)
        {
            p.icon(s.icon, sx + 22, 650, 44, tone::sky_muted);
            const float vw = p.number(s.value, sx + 56, 664, size::numeral, tone::text);
            const float cw = p.measure(s.caption, size::caption);
            p.text(s.caption, sx + 56, 704, size::caption, tone::sky_muted);
            sx += 56 + std::max(vw, cw) + 44;
        }
    }
    // The next 12 hours.
    const Rect right{656, size::content_top, 1920 - size::margin - 656,
                     size::content_bottom - size::content_top};
    if (state == "failed")
    {
        p.panel(right, size::tile_radius, tone::well);
        p.failed(right, "the forecast", true);
        return;
    }
    const auto hours = next_hours(forecast_.hours, now());
    const bool rain =
        std::any_of(hours.begin(), hours.end(), [](const Hour &h) { return h.rain >= 0.1; });
    const float h2 =
        p.text("Next 12 hours", right.x + 4, right.y + 30, size::label, tone::sky_muted, true);
    if (state == "ready")
        p.text(rain ? "rain in mm" : "no rain expected", right.x + 4 + h2 + 18, right.y + 30,
               size::caption, tone::sky_muted);
    const Rect chart{right.x, right.y + 52, right.w, 352};
    p.panel(chart, size::tile_radius, tone::well);
    if (state != "ready")
        p.text(state == "loading" ? "Loading forecast\xE2\x80\xA6"
                                  : "No hourly forecast from this weather service",
               chart.x + 36, chart.y + 70, size::label, tone::sky_muted);
    else
    {
        double lo = hours[0].temperature, hi = lo, wettest = 1;
        for (const auto &h : hours)
        {
            lo = std::min(lo, h.temperature);
            hi = std::max(hi, h.temperature);
            wettest = std::max(wettest, h.rain);
        }
        if (hi - lo < 3)
            lo -= 1.5, hi += 1.5;
        const auto n = static_cast<float>(hours.size());
        const float column = (chart.w - 24) / n;
        auto px = [&](std::size_t i)
        { return chart.x + 12 + (static_cast<float>(i) + 0.5f) * column; };
        auto py = [&](double t)
        { return chart.y + 92 + static_cast<float>((1 - (t - lo) / (hi - lo)) * 96); };
        const auto colour =
            chart_line(sky_for(w.state, snapshot_.states.get("sun.sun").state != "below_horizon"));
        for (std::size_t i = 0; i < hours.size(); ++i)
        {
            const auto &h = hours[i];
            const float x = px(i), y = py(h.temperature);
            if (i + 1 < hours.size())
                p.list.line(x, y, px(i + 1), py(hours[i + 1].temperature), 5, colour);
            p.text(degrees(h.temperature), x, y - 22, size::label, tone::text, true, Align::center);
            if (h.rain >= 0.1)
            {
                const float bar = 8 + static_cast<float>(h.rain / wettest) * 54;
                p.list.rounded_rect({x - column * 0.3f, chart.y + 262 - bar, column * 0.6f, bar}, 5,
                                    tone::accent.with_alpha(0.5f));
                p.text(one_decimal(h.rain), x, chart.y + 252 - bar, size::caption, tone::accent,
                       false, Align::center);
            }
            p.icon(condition_icon(h.condition), x, chart.y + 290, 40, tone::sky_muted);
            p.number(i == 0 ? "Now" : two_digit_hour(h.at, clock_.zone), x, chart.y + 340,
                     size::caption, tone::sky_muted, Align::center, true);
        }
    }
    // The next five days, low to high on one shared scale.
    std::vector<Day> days;
    for (const auto &d : forecast_.days)
        if (day_start(d.at, clock_.zone) > day_start(now(), clock_.zone) && days.size() < 5)
            days.push_back(d);
    if (days.empty())
        return;
    const Rect box{right.x, chart.y + chart.h + 22, right.w,
                   right.y + right.h - (chart.y + chart.h + 22)};
    p.panel(box, size::tile_radius, tone::well);
    p.text("Next days", box.x + 32, box.y + 44, size::label, tone::sky_muted, true);
    double lo = days[0].low, hi = days[0].high;
    for (const auto &d : days)
    {
        lo = std::min(lo, d.low);
        hi = std::max(hi, d.high);
    }
    const double span = std::max(1.0, hi - lo);
    const float row = std::min(52.0f, (box.h - 70) / static_cast<float>(days.size()));
    for (std::size_t i = 0; i < days.size(); ++i)
    {
        const auto &d = days[i];
        const float base = box.y + 70 + row * static_cast<float>(i) + row * 0.68f;
        const float cy = base - size::body * 0.34f;
        p.text(weekday_short(local(d.at, clock_.zone).weekday), box.x + 32, base, size::body,
               tone::text, true);
        p.icon(condition_icon(d.condition), box.x + 150, cy, 36, tone::sky_muted);
        if (d.rain >= 0.2)
            p.text(one_decimal(d.rain) + " mm", box.x + 196, base, size::caption, tone::accent);
        p.text(degrees(d.low), box.x + 430, base, size::body, tone::sky_muted, false, Align::right);
        const float bar_x = box.x + 456, bar_w = box.w - 456 - 120;
        p.list.rounded_rect({bar_x, cy - 6, bar_w, 12}, 6, Color::rgb(0xffffff, 0.14f));
        const float from = static_cast<float>((d.low - lo) / span) * bar_w;
        const float width =
            std::max(bar_w * 0.04f, static_cast<float>((d.high - d.low) / span) * bar_w);
        p.list.gradient_rect_h({bar_x + from, cy - 6, width, 12}, 6, tone::accent, tone::warm);
        p.text(degrees(d.high), box.x + box.w - 32, base, size::body, tone::text, true,
               Align::right);
    }
}
} // namespace lamplight
