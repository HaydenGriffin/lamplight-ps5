// Lamplight for PS5 - A sheet's chart: a reading's last 24 hours, and what it is set to.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace lamplight
{
using hui::gfx::Align;
namespace
{
constexpr float kAxisW = 76, kTitleH = 70, kTicksH = 48, kInset = 28;
constexpr float kLineW = 4, kDash = 14, kDashGap = 10;
bool in_degrees(const std::string &unit)
{
    return unit.starts_with("\xC2\xB0");
}
// A value with its unit where the unit is short enough to stay with it.
std::string chart_words(double value, const std::string &unit, bool whole)
{
    char digits[32];
    std::snprintf(digits, sizeof digits, whole || value == std::floor(value) ? "%.0f" : "%.1f",
                  value);
    return digits + std::string(in_degrees(unit) ? "\xC2\xB0" : unit == "%" ? "%" : "");
}
// The hour marks: whole hours six apart (18:00, 00:00, 06:00, 12:00), none so near either end
// that its label runs into the axis or into "Now".
std::vector<std::int64_t> chart_hours(std::int64_t from, std::int64_t to, const Zone &zone)
{
    constexpr std::int64_t kHourMs = 3600000, kStepMs = kHourMs / 4;
    std::vector<std::int64_t> marks;
    for (std::int64_t at = from - from % kStepMs + kStepMs; at < to - 3 * kHourMs; at += kStepMs)
    {
        const auto when = local(at, zone);
        if (at > from + kHourMs && when.minute == 0 && when.hour % 6 == 0)
            marks.push_back(at);
    }
    return marks;
}
} // namespace

void Application::draw_chart(Paint &p, const Control &row, const Rect &r) const
{
    p.list.rounded_rect(r, size::small_radius, tone::well);
    const bool own_unit = !row.unit.empty() && !in_degrees(row.unit) && row.unit != "%";
    p.text(own_unit ? row.label + " \xC2\xB7 " + row.unit : row.label, r.x + kInset, r.y + 46,
           size::label, tone::sky_muted, true);
    static const std::vector<HistoryPoint> none;
    const auto &points = chart_.entity == row.entity ? chart_.points : none;
    if (points.empty())
    {
        const bool failed = chart_.entity == row.entity && chart_.failed;
        const bool waiting = chart_.entity != row.entity || chart_.loading || !chart_.at;
        p.text(failed    ? "Couldn't load the last 24 hours"
               : waiting ? "Loading the last 24 hours\xE2\x80\xA6"
                         : "No history for the last 24 hours",
               r.x + kInset, r.cy() + 12, size::label, failed ? tone::warn : tone::sky_muted);
        return;
    }
    double low = points[0].value, high = low;
    for (const auto &point : points)
    {
        low = std::min(low, point.value);
        high = std::max(high, point.value);
    }
    p.text("Low " + chart_words(low, row.unit, false) + " \xC2\xB7 High " +
               chart_words(high, row.unit, false),
           r.x + r.w - kInset, r.y + 46, size::caption, tone::sky_muted, false, Align::right);
    const Rect plot{r.x + kInset + kAxisW, r.y + kTitleH, r.w - 2 * kInset - kAxisW,
                    r.h - kTitleH - kTicksH};
    const auto scale = chart_scale(points, row.target);
    const std::int64_t from = now() - kChartHoursMs;
    auto x_at = [&](std::int64_t at)
    {
        const auto since = std::clamp<std::int64_t>(at - from, 0, kChartHoursMs);
        return plot.x + plot.w * static_cast<float>(since) / static_cast<float>(kChartHoursMs);
    };
    auto y_at = [&](double value)
    {
        const double range = std::max(scale.high - scale.low, 1e-6);
        return plot.y + plot.h * static_cast<float>((scale.high - value) / range);
    };
    for (const double mark : {scale.high, (scale.high + scale.low) / 2, scale.low})
    {
        p.list.line(plot.x, y_at(mark), plot.x + plot.w, y_at(mark), 2, tone::line);
        p.number(chart_words(mark, row.unit, true), plot.x - 16, y_at(mark) + 10, size::caption,
                 tone::sky_muted, Align::right, true);
    }
    for (const auto at : chart_hours(from, now(), clock_.zone))
        p.number(hhmm(at, clock_.zone), x_at(at), plot.y + plot.h + 38, size::caption,
                 tone::sky_muted, Align::center, true);
    p.text("Now", plot.x + plot.w, plot.y + plot.h + 38, size::caption, tone::sky_muted, false,
           Align::right);
    if (!std::isnan(row.target))
    {
        const float y = y_at(row.target);
        for (float x = plot.x; x < plot.x + plot.w; x += kDash + kDashGap)
            p.list.line(x, y, std::min(x + kDash, plot.x + plot.w), y, 3, tone::accent);
        const auto words = "Set " + chart_words(row.target, row.unit, false);
        const bool above = y - plot.y > 40;
        p.text(words, plot.x + plot.w, above ? y - 12 : y + 36, size::caption, tone::accent, false,
               Align::right);
    }
    // In steps, as the reading stayed where it was until the next one came.
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const float x = x_at(points[i].at), y = y_at(points[i].value);
        const float next = i + 1 < points.size() ? x_at(points[i + 1].at) : plot.x + plot.w;
        p.list.line(x, y, next, y, kLineW, tone::warm);
        if (i + 1 < points.size())
            p.list.line(next, y, next, y_at(points[i + 1].value), kLineW, tone::warm);
    }
}
} // namespace lamplight
