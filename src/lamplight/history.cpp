// Lamplight for PS5 - A reading's last 24 hours from Home Assistant's history, for a sheet's chart.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/history.hpp"
#include <algorithm>
#include <cmath>

namespace lamplight
{
namespace
{
constexpr std::int64_t kBucketMs = 5 * 60000;
constexpr std::size_t kMostPoints = 300;
// Its time in ms: Home Assistant leaves "lu" out of a row when only "lc" says it.
std::int64_t row_time(const Json &row)
{
    const auto &updated = field(row, "lu");
    const auto &seconds = updated.is_number() ? updated : field(row, "lc");
    return seconds.is_number() ? static_cast<std::int64_t>(seconds.get<double>() * 1000) : 0;
}
} // namespace

Request history_request(const std::string &entity, const std::string &attribute, std::int64_t now,
                        const Zone &zone)
{
    const bool own_state = attribute.empty();
    Json message{{"type", "history/history_during_period"},
                 {"start_time", iso_local(now - kChartHoursMs, zone)},
                 {"entity_ids", Json::array({entity})},
                 {"minimal_response", own_state},
                 {"no_attributes", own_state},
                 {"significant_changes_only", false}};
    Request request{std::move(message), 25000000};
    request.history_entity = entity;
    request.history_attribute = attribute;
    return request;
}
Json compact_history(const Json &result, const std::string &entity, const std::string &attribute)
{
    auto out = Json::array();
    const auto &rows = field(result, entity);
    if (rows.is_array())
    {
        std::int64_t bucket = -1;
        for (const auto &row : rows)
        {
            const auto at = row_time(row);
            const double value = attribute.empty() ? number(field(row, "s"))
                                                   : number(field(field(row, "a"), attribute));
            if (at <= 0 || std::isnan(value) || std::isinf(value))
                continue;
            // The last reading in each five minutes stands for them.
            if (at / kBucketMs == bucket && !out.empty())
                out.back() = Json::array({at, value});
            else if (out.size() < kMostPoints)
                out.push_back(Json::array({at, value}));
            bucket = at / kBucketMs;
        }
    }
    return {{"points", std::move(out)}};
}
std::vector<HistoryPoint> history_points(const Json &compact)
{
    std::vector<HistoryPoint> points;
    const auto &list = field(compact, "points");
    if (list.is_array())
        for (const auto &p : list)
            if (p.is_array() && p.size() == 2 && p[0].is_number() && p[1].is_number())
                points.push_back({p[0].get<std::int64_t>(), p[1].get<double>()});
    std::stable_sort(points.begin(), points.end(),
                     [](const HistoryPoint &a, const HistoryPoint &b) { return a.at < b.at; });
    return points;
}
ChartScale chart_scale(const std::vector<HistoryPoint> &points, double target)
{
    double low = std::isnan(target) ? INFINITY : target,
           high = std::isnan(target) ? -INFINITY : target;
    for (const auto &p : points)
    {
        low = std::min(low, p.value);
        high = std::max(high, p.value);
    }
    if (low > high)
        return {};
    if (high - low < 2)
    {
        low -= 1;
        high += 1;
    }
    return {std::floor(low), std::ceil(high)};
}
} // namespace lamplight
