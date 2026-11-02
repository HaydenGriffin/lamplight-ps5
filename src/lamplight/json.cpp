// Lamplight for PS5 - Defensive reads from Home Assistant's JSON.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/json.hpp"
#include "lamplight/decimal.hpp"
#include <cmath>

namespace lamplight
{
const Json &field(const Json &j, const std::string &key)
{
    static const Json empty;
    if (!j.is_object())
        return empty;
    auto it = j.find(key);
    return it == j.end() ? empty : *it;
}
std::string text(const Json &j, const std::string &key, const std::string &fallback)
{
    const auto &v = field(j, key);
    return v.is_string() ? v.get<std::string>() : fallback;
}
std::vector<std::string> strings(const Json &j)
{
    std::vector<std::string> out;
    if (j.is_array())
        for (const auto &v : j)
            if (v.is_string())
                out.push_back(v.get<std::string>());
    return out;
}
double number(const Json &j)
{
    if (j.is_number())
        return j.get<double>();
    if (j.is_string())
    {
        // As JavaScript's parseFloat: leading space is skipped, and no digits is no number.
        const char *start = j.get_ref<const std::string &>().c_str();
        while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')
            ++start;
        char *end = nullptr;
        const double value = parse_decimal(start, &end);
        return end == start ? NAN : value;
    }
    return NAN;
}
bool has_number(const Json &j)
{
    return std::isfinite(number(j));
}
} // namespace lamplight
