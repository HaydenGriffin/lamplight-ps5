// Lamplight for PS5 - Settings: the Home Assistant connection and what each page shows.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/config.hpp"
#include <arpa/inet.h>
#include <algorithm>
#include <sys/socket.h>

namespace lamplight
{
namespace
{
std::vector<Choice> choices(const Json &j)
{
    std::vector<Choice> out;
    if (j.is_array())
        for (const auto &v : j)
        {
            if (v.is_string())
                out.push_back({v.get<std::string>(), "", ""});
            else if (!text(v, "entity").empty())
                out.push_back({text(v, "entity"), text(v, "name"), text(v, "icon")});
        }
    // The cards' map form: {calendar.family: {name: Family, icon: mdi:home-heart}}.
    else if (j.is_object())
        for (auto it = j.begin(); it != j.end(); ++it)
            out.push_back({it.key(), text(*it, "name"), text(*it, "icon")});
    if (out.size() > 24)
        out.resize(24);
    return out;
}
Json choices_json(const std::vector<Choice> &list)
{
    auto out = Json::array();
    for (const auto &c : list)
    {
        Json item{{"entity", c.entity}};
        if (!c.name.empty())
            item["name"] = c.name;
        if (!c.icon.empty())
            item["icon"] = c.icon;
        out.push_back(std::move(item));
    }
    return out;
}
std::vector<HelpCategory> parse_categories(const Json &j)
{
    std::vector<HelpCategory> out;
    if (!j.is_array())
        return out;
    for (const auto &c : j)
    {
        HelpCategory cat{text(c, "name"), text(c, "icon"), text(c, "hint"), text(c, "tip"), {}};
        if (cat.name.empty())
            continue;
        const auto &examples = field(c, "examples");
        if (examples.is_array())
            for (const auto &e : examples)
            {
                HelpExample ex{text(e, "say"), text(e, "run"), text(e, "agent"), text(e, "note")};
                ex.confirm = field(e, "confirm") == true;
                ex.tap = field(e, "tap") != false;
                if (!ex.say.empty() && cat.examples.size() < 40)
                    cat.examples.push_back(std::move(ex));
            }
        if (out.size() < 24)
            out.push_back(std::move(cat));
    }
    return out;
}
Json categories_json(const std::vector<HelpCategory> &list)
{
    auto out = Json::array();
    for (const auto &c : list)
    {
        auto examples = Json::array();
        for (const auto &e : c.examples)
        {
            Json ex{{"say", e.say}};
            if (!e.run.empty())
                ex["run"] = e.run;
            if (!e.agent.empty())
                ex["agent"] = e.agent;
            if (!e.note.empty())
                ex["note"] = e.note;
            if (e.confirm)
                ex["confirm"] = true;
            if (!e.tap)
                ex["tap"] = false;
            examples.push_back(std::move(ex));
        }
        out.push_back({{"name", c.name},
                       {"icon", c.icon},
                       {"hint", c.hint},
                       {"tip", c.tip},
                       {"examples", std::move(examples)}});
    }
    return out;
}
std::vector<RoomThing> things(const Json &j)
{
    std::vector<RoomThing> out;
    if (j.is_array())
        for (const auto &t : j)
        {
            RoomThing thing{text(t, "entity"), text(t, "name"), text(t, "icon"),
                            field(t, "confirm") == true};
            if (!thing.entity.empty() && out.size() < 12)
                out.push_back(std::move(thing));
        }
    return out;
}
Json things_json(const std::vector<RoomThing> &list)
{
    auto out = Json::array();
    for (const auto &t : list)
    {
        Json item{{"entity", t.entity}};
        if (!t.name.empty())
            item["name"] = t.name;
        if (!t.icon.empty())
            item["icon"] = t.icon;
        if (t.confirm)
            item["confirm"] = true;
        out.push_back(std::move(item));
    }
    return out;
}
} // namespace

bool valid_ipv4(const std::string &host)
{
    in_addr address{};
    return inet_pton(AF_INET, host.c_str(), &address) == 1;
}
bool Config::configured() const
{
    return valid_ipv4(host) && port > 0 && port <= 65535 && !token.empty() &&
           token.size() <= 2048 && token.find_first_of("\r\n") == std::string::npos;
}
Json Config::json() const
{
    Json j{
        {"host", host},
        {"port", port},
        {"token", token},
        {"weather", weather},
        {"calendars", choices_json(calendars)},
        {"view_assist", view_assist},
        {"quick_minutes", quick_minutes},
        {"appliance_timers", choices_json(appliances)},
        {"wifi", {{"ssid", wifi_ssid}, {"password", wifi_password}, {"security", wifi_security}}},
        {"agent_id", agent},
        {"wake_phrase", wake_phrase},
        {"room", room},
        {"read_only_entity", read_only},
        {"reduced_motion", reduced_motion},
        {"vibration", vibration}};
    if (!categories.empty())
        j["categories"] = categories_json(categories);
    if (!room_page.empty())
        j["room_page"] = {{"name", room_page.name},
                          {"icon", room_page.icon},
                          {"media", room_page.media},
                          {"tiles", things_json(room_page.tiles)},
                          {"actions", things_json(room_page.actions)}};
    return j;
}
Config Config::parse(const Json &j)
{
    Config c;
    c.host = text(j, "host");
    c.token = text(j, "token");
    const auto &port = field(j, "port");
    if (port.is_number_integer() && port >= 1 && port <= 65535)
        c.port = port.get<int>();
    c.weather = text(j, "weather");
    c.calendars = choices(field(j, "calendars"));
    c.view_assist = text(j, "view_assist", text(j, "view_assist_sensor"));
    const auto &quick = field(j, "quick_minutes");
    if (quick.is_array())
    {
        c.quick_minutes.clear();
        for (const auto &m : quick)
            if (m.is_number_integer() && m >= 1 && m <= 600 && c.quick_minutes.size() < 6)
                c.quick_minutes.push_back(m.get<int>());
    }
    c.appliances = choices(field(j, "appliance_timers"));
    const auto &wifi = field(j, "wifi");
    c.wifi_ssid = text(wifi, "ssid");
    c.wifi_password = text(wifi, "password");
    c.wifi_security = text(wifi, "security", "WPA");
    c.agent = text(j, "agent_id", c.agent);
    c.wake_phrase = text(j, "wake_phrase", c.wake_phrase);
    c.room = text(j, "room");
    c.read_only = text(j, "read_only_entity");
    c.categories = parse_categories(field(j, "categories"));
    const auto &room = field(j, "room_page");
    c.room_page = {text(room, "name"), text(room, "icon"), text(room, "media"),
                   things(field(room, "tiles")), things(field(room, "actions"))};
    if (c.room_page.actions.size() > 6)
        c.room_page.actions.resize(6);
    c.reduced_motion = field(j, "reduced_motion") == true;
    c.vibration = field(j, "vibration") == true;
    return c;
}
const std::vector<HelpCategory> &Config::help() const
{
    return categories.empty() ? default_categories() : categories;
}
const std::vector<HelpCategory> &default_categories()
{
    static const std::vector<HelpCategory> list = {
        {"Lights",
         "mdi:lightbulb-group",
         "Rooms, brightness and scenes",
         "Say the room's name, or nothing for the room you're in.",
         {{"turn on the lights in the {room}", "", "", "", false, true},
          {"dim the lights to 30 percent", "", "", "", false, true},
          {"turn off the lights in the kitchen", "", "", "", false, true},
          {"turn everything off", "", "", "", true, true}}},
        {"Timers",
         "mdi:timer-outline",
         "Countdowns, alarms and reminders",
         "Timers you set by voice show on the Timers page.",
         {{"set a timer for 10 minutes", "", "", "", false, true},
          {"set a pasta timer for 12 minutes", "", "", "", false, true},
          {"how long is left on the timer", "", "", "", false, true},
          {"cancel the timer", "", "", "", false, true}}},
        {"Weather",
         "mdi:weather-partly-cloudy",
         "Today, tomorrow and the week",
         "",
         {{"what's the weather like", "", "", "", false, true},
          {"will it rain tomorrow", "", "", "", false, true},
          {"what's the temperature outside", "", "", "", false, true}}},
        {"Music",
         "mdi:music",
         "Play, pause and volume",
         "",
         {{"play some music in the {room}", "", "", "", false, true},
          {"pause the music", "", "", "", false, true},
          {"turn the volume down", "", "", "", false, true}}},
        {"Questions",
         "mdi:comment-question-outline",
         "Time, dates and the house",
         "",
         {{"what time is it", "", "", "", false, true},
          {"what's the date today", "", "", "", false, true},
          {"is the front door locked", "", "", "", false, true},
          {"remember that…", "", "", "It needs your own words", false, false}}},
    };
    return list;
}
std::string fill_room(const std::string &text, const std::string &room)
{
    auto replace_all = [](std::string s, const std::string &from, const std::string &to)
    {
        for (std::size_t at = s.find(from); at != std::string::npos;
             at = s.find(from, at + to.size()))
            s.replace(at, from.size(), to);
        return s;
    };
    if (!room.empty())
        return replace_all(text, "{room}", room);
    auto out = replace_all(text, ", in the {room}", "");
    out = replace_all(out, " in the {room}", "");
    return replace_all(out, "{room}", "here");
}
} // namespace lamplight
