// Lamplight for PS5 - Live entity states from Home Assistant's subscribe_entities.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/json.hpp"
#include <map>

namespace lamplight
{
struct Entity
{
    std::string state;
    Json attributes = Json::object();
    bool available() const;
    std::string name(const std::string &fallback) const;
};
class StateTable
{
  public:
    // One subscribe_entities event: "a" adds, "c" changes ("+" and "-"), "r" removes.
    void apply(const Json &event);
    const Entity &get(const std::string &id) const;
    bool has(const std::string &id) const
    {
        return states_.contains(id);
    }
    void clear()
    {
        states_.clear();
    }

  private:
    std::map<std::string, Entity> states_;
};
// An entity Setup can offer, from get_states: only the domains Lamplight reads.
struct CatalogEntry
{
    std::string id, name;
    bool has_timers = false; // a sensor with a "timers" attribute: a View Assist device
};
std::vector<CatalogEntry> catalog(const Json &states);
std::string domain_of(const std::string &entity);
} // namespace lamplight
