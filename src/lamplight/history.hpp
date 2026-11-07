// Lamplight for PS5 - A reading's last 24 hours from Home Assistant's history, for a sheet's chart.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/client.hpp"
#include "lamplight/clock.hpp"

namespace lamplight
{
inline constexpr std::int64_t kChartHoursMs = 24 * 3600000;
struct HistoryPoint
{
    std::int64_t at = 0;
    double value = 0;
};
// The chart a sheet is showing: what's been read of it, and when.
struct History
{
    std::string entity;
    std::vector<HistoryPoint> points;
    std::int64_t at = 0, asked = 0;
    bool loading = false, failed = false;
};
// Its state over the last day, or the attribute when it has one (a thermostat's own reading).
Request history_request(const std::string &entity, const std::string &attribute, std::int64_t now,
                        const Zone &zone);
// Home Assistant's rows made small enough to keep: at most one reading per five minutes, as
// {"points": [[ms, value], ...]}. It runs on the network thread, so nothing large reaches a frame.
Json compact_history(const Json &result, const std::string &entity, const std::string &attribute);
std::vector<HistoryPoint> history_points(const Json &compact);
// The range a chart's axis shows: whole numbers around what's there, at least two apart.
struct ChartScale
{
    double low = 0, high = 1;
};
ChartScale chart_scale(const std::vector<HistoryPoint> &points, double target);
} // namespace lamplight
