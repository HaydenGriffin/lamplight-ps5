// Lamplight for PS5 - The payload's view of the console: the game in front, opening a title, rest.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace lamplight::console
{
struct Game
{
    std::string title_id, name; // both empty when nothing (or only the system) is in front
    int app_id = 0;
};
enum class Launch
{
    opened,
    busy, // a game is in front, and a game is never closed for another
    failed
};
// The title running in front (a game or an app), with its name from the system's copy of its
// metadata.
Game running_game();
// PPSA01234 or CUSA01234: four capitals and five digits.
bool valid_title_id(const std::string &id);
// Opens a title for the user in front, or brings it to the front if it is running. Lamplight in
// front is closed first (the system opens one app at a time); a game in front is left alone.
Launch launch(const std::string &title_id);
// Puts the console in rest mode.
bool rest();
// A title's name from its param.json (PS5) or param.sfo (PS4); "" when neither can be read.
std::string title_name(const std::string &title_id);
// The TITLE of a param.sfo, "" when it has none.
std::string sfo_title(const std::uint8_t *data, std::size_t size);
} // namespace lamplight::console
