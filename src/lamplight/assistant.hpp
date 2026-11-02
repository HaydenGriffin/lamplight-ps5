// Lamplight for PS5 - The assistant's replies: misfires and light markdown.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>

namespace lamplight
{
// A short reply that says the assistant didn't get it ("couldn't understand", a name it didn't
// know, the greeting after a bare wake word, or only the wake phrase). Only replies under 140
// characters are checked, so an answer is never taken for a misfire. As the Info card's isMisfire.
bool is_misfire(const std::string &reply, const std::string &wake_phrase);
// **bold** markers dropped and "* " or "- " bullets as "• ".
std::string plain_reply(const std::string &reply);
} // namespace lamplight
