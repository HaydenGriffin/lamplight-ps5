// Lamplight for PS5 - Live entity states from Home Assistant's subscribe_entities.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/states.hpp"
#include <algorithm>

namespace lamplight
{
bool Entity::available() const
{
    return !state.empty() && state != "unavailable" && state != "unknown";
}
std::string Entity::name(const std::string &fallback) const
{
    return text(attributes, "friendly_name", fallback);
}
const Entity &StateTable::get(const std::string &id) const
{
    static const Entity absent;
    auto it = states_.find(id);
    return it == states_.end() ? absent : it->second;
}
void StateTable::apply(const Json &event)
{
    const auto &added = field(event, "a");
    if (added.is_object())
        for (auto it = added.begin(); it != added.end(); ++it)
        {
            Entity e;
            e.state = text(*it, "s");
            if (field(*it, "a").is_object())
                e.attributes = field(*it, "a");
            states_[it.key()] = std::move(e);
        }
    const auto &changed = field(event, "c");
    if (changed.is_object())
        for (auto it = changed.begin(); it != changed.end(); ++it)
        {
            auto found = states_.find(it.key());
            if (found == states_.end())
                continue;
            auto &e = found->second;
            const auto &plus = field(*it, "+");
            if (field(plus, "s").is_string())
                e.state = text(plus, "s");
            if (field(plus, "a").is_object())
                e.attributes.update(field(plus, "a"));
            for (const auto &key : strings(field(field(*it, "-"), "a")))
                e.attributes.erase(key);
        }
    for (const auto &id : strings(field(event, "r")))
        states_.erase(id);
}
std::string domain_of(const std::string &entity)
{
    auto dot = entity.find('.');
    return dot == std::string::npos ? "" : entity.substr(0, dot);
}
std::vector<CatalogEntry> catalog(const Json &states)
{
    std::vector<CatalogEntry> out;
    if (!states.is_array())
        return out;
    for (const auto &s : states)
    {
        auto id = text(s, "entity_id");
        auto domain = domain_of(id);
        const auto &attributes = field(s, "attributes");
        const bool timers = domain == "sensor" && field(attributes, "timers").is_array();
        if (!(domain == "weather" || domain == "calendar" || domain == "timer" ||
              domain == "conversation" || domain == "input_boolean" || domain == "switch" ||
              timers))
            continue;
        out.push_back({id, text(attributes, "friendly_name", id), timers});
    }
    std::sort(out.begin(), out.end(),
              [](const CatalogEntry &a, const CatalogEntry &b) { return a.id < b.id; });
    return out;
}
} // namespace lamplight
