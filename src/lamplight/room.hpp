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
// A thermostat whose tile has warm-ups is run by the house's heating plan, which sets its radiator
// itself (25° to warm a room to 21°): Lamplight shows the room and the warm-ups, and leaves the
// radiator's own target and mode alone.
bool plan_runs(const std::string &entity, const std::vector<NamedCall> &boosts);
// A tile's words: its state's, but a thermostat reads its room sensor when it has one ("19.5°"),
// and one the plan runs doesn't show the radiator's target.
std::string thing_words(const std::string &entity, const Entity &state, const Entity &sensor,
                        bool plan);
const char *default_icon(const std::string &entity, const Entity &state = {});
// Whether a tile can glow, rather than only report or run: the room's "3 of 6 on" counts exactly
// these, so it always agrees with the tiles that are lit.
bool can_glow(const std::string &entity);
} // namespace lamplight
