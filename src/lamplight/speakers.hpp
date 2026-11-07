// Lamplight for PS5 - Which speakers play together, and the rows of the Speakers sheet.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/clock.hpp"
#include "lamplight/controls.hpp"

namespace lamplight
{
// A speaker in the group while the group plays is the group, as on the tablets: the same title
// and cover, and the controls drive the group, whose volume keeps every room's balance.
bool group_active(const Speakers &speakers, const StateTable &states);
bool in_session(const Speakers &speakers, const StateTable &states, const std::string &player);
struct MusicSession
{
    std::string target; // the entity the controls act on
    bool together = false;
    Entity state; // as people should see it
};
MusicSession music_session(const Speakers &speakers, const StateTable &states,
                           const std::string &player);
// The speakers set up in settings, or each room page's own.
std::vector<Speaker> speaker_list(const Config &config);
// "Everywhere · 3 rooms", "Playing in 2 rooms" or "Nothing playing".
std::string speakers_words(const Config &config, const StateTable &states);
// Here is the speaker the page is showing. moving: a move is on its way, so nothing starts another.
std::vector<Control> speaker_rows(const Config &config, const StateTable &states,
                                  const std::string &here, bool moving, std::int64_t now,
                                  const Zone &zone);
} // namespace lamplight
