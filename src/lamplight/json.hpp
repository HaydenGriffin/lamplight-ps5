// Lamplight for PS5 - Defensive reads from Home Assistant's JSON.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "third_party/json.hpp"
#include <string>
#include <vector>

namespace lamplight
{
using Json = nlohmann::json;
// The member, or a null that is safe to read further.
const Json &field(const Json &j, const std::string &key);
std::string text(const Json &j, const std::string &key, const std::string &fallback = "");
std::vector<std::string> strings(const Json &j);
// A number, or a numeric string such as "12.5" (some integrations send strings). NaN when neither.
double number(const Json &j);
bool has_number(const Json &j);
} // namespace lamplight
