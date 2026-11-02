// Lamplight for PS5 - The room page's things: what pressing one does, and how it reads.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/client.hpp"
#include <optional>

namespace lamplight
{
// The service a press calls, by the entity's domain: lights, switches, fans and helpers toggle,
// scenes and scripts run, a media player plays or pauses, a cover opens or closes. Nothing for a
// thing that only reports (a thermostat, a sensor).
std::optional<Request> press_action(const std::string &entity);
// "On" in Lamplight's sense: lit, running, playing, open (The Lamp Rule).
bool is_on(const std::string &entity, const Entity &state);
// The state in words: "On", "Off", "40%", "Playing", "21.5°", "Open", "Unavailable".
std::string state_words(const std::string &entity, const Entity &state);
const char *default_icon(const std::string &entity);
} // namespace lamplight
