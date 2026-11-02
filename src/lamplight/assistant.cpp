// Lamplight for PS5 - The assistant's replies: misfires and light markdown.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/assistant.hpp"
#include <vector>

namespace lamplight
{
namespace
{
std::string lower(std::string s)
{
    for (auto &c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    // Curly apostrophes read as straight ones.
    for (std::size_t at = s.find("\xE2\x80\x99"); at != std::string::npos;
         at = s.find("\xE2\x80\x99"))
        s.replace(at, 3, "'");
    return s;
}
std::string trim(const std::string &s, const char *set = " \t\r\n")
{
    const auto first = s.find_first_not_of(set);
    if (first == std::string::npos)
        return "";
    return s.substr(first, s.find_last_not_of(set) - first + 1);
}
bool has(const std::string &s, const char *part)
{
    return s.find(part) != std::string::npos;
}
} // namespace

bool is_misfire(const std::string &reply, const std::string &wake_phrase)
{
    const auto t = trim(reply);
    if (t.empty() || t.size() >= 140)
        return false;
    const auto l = lower(t);
    if (has(l, "hasn't been defined") || has(l, "hasnt been defined") ||
        has(l, "has not been defined") || has(l, "isn't defined") || has(l, "isnt defined") ||
        has(l, "couldn't understand") || has(l, "couldnt understand") || has(l, "not sure what") ||
        has(l, "not aware of") || has(l, "unexpected error"))
        return true;
    for (const char *start :
         {"sorry, i didn't", "sorry i didn't", "sorry, i did not", "sorry i did not",
          "sorry, i can't", "sorry i can't", "sorry, i cannot", "sorry i cannot", "sorry, i didnt",
          "sorry i didnt", "sorry, i cant", "sorry i cant"})
        if (l.starts_with(start))
            return true;
    // "No device named X", "No timers found".
    if (l.starts_with("no ") && (has(l, " named ") || has(l, " called ") || has(l, " found")))
        for (const char *what : {"no device", "no entity", "no entities", "no area", "no timer"})
            if (l.starts_with(what))
                return true;
    for (const char *greeting :
         {"hello! i'm here to help", "hello, i'm here to help", "hello. i'm here to help",
          "hello i'm here to help", "hi! i'm here to help", "hi, i'm here to help",
          "hi i'm here to help", "hello! i'm ready to help", "hi! i'm ready to help",
          "hello, i'm ready to help"})
        if (l.starts_with(greeting))
            return true;
    for (const char *ending : {"what can i do for you?", "what can i do for you",
                               "what can i do for you today?", "what can i do for you today"})
        if (l.ends_with(ending))
            return true;
    const auto wake = lower(trim(wake_phrase));
    return !wake.empty() && trim(l, " \t\r\n\"'.!?,") == wake;
}
std::string plain_reply(const std::string &reply)
{
    std::string out;
    std::size_t start = 0;
    const auto t = trim(reply);
    while (start <= t.size())
    {
        auto end = t.find('\n', start);
        auto line = t.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const auto lead = line.find_first_not_of(" \t");
        if (lead != std::string::npos && lead + 1 < line.size() &&
            (line[lead] == '*' || line[lead] == '-') &&
            (line[lead + 1] == ' ' || line[lead + 1] == '\t'))
            line = "\xE2\x80\xA2 " + trim(line.substr(lead + 2));
        out += line;
        if (end == std::string::npos)
            break;
        out += '\n';
        start = end + 1;
    }
    for (std::size_t at = out.find("**"); at != std::string::npos; at = out.find("**", at))
        out.erase(at, 2);
    return out;
}
} // namespace lamplight
