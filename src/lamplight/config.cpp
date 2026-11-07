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
// {"service": "script.x", "data": {...}, "target": "light.y", "response": true}; empty unless the
// service is written as domain.name.
ServiceCall service_call(const Json &j)
{
    ServiceCall call{text(j, "service"), Json::object(), text(j, "target"),
                     field(j, "response") == true};
    if (call.service.find('.') == std::string::npos)
        return {};
    if (field(j, "data").is_object())
        call.data = field(j, "data");
    return call;
}
void put_call(Json &item, const ServiceCall &call)
{
    if (call.empty())
        return;
    item["service"] = call.service;
    if (!call.data.empty())
        item["data"] = call.data;
    if (!call.entity.empty())
        item["target"] = call.entity;
    if (call.response)
        item["response"] = true;
}
Json call_json(const ServiceCall &call)
{
    auto item = Json::object();
    put_call(item, call);
    return item;
}
std::vector<NamedCall> named_calls(const Json &j, std::size_t most)
{
    std::vector<NamedCall> out;
    if (j.is_array())
        for (const auto &c : j)
        {
            NamedCall named{text(c, "name"), text(c, "icon"), service_call(c),
                            field(c, "confirm") == true, field(c, "undo") == true};
            if (!named.name.empty() && !named.call.empty() && out.size() < most)
                out.push_back(std::move(named));
        }
    return out;
}
Json named_calls_json(const std::vector<NamedCall> &list)
{
    auto out = Json::array();
    for (const auto &c : list)
    {
        Json item{{"name", c.name}};
        if (!c.icon.empty())
            item["icon"] = c.icon;
        put_call(item, c.call);
        if (c.confirm)
            item["confirm"] = true;
        if (c.undo_lights)
            item["undo"] = true;
        out.push_back(std::move(item));
    }
    return out;
}
std::vector<RoomThing> things(const Json &j)
{
    std::vector<RoomThing> out;
    if (j.is_array())
        for (const auto &t : j)
        {
            RoomThing thing{text(t, "entity"), text(t, "name"),
                            text(t, "icon"),   field(t, "confirm") == true,
                            text(t, "sensor"), named_calls(field(t, "boosts"), 6)};
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
        if (!t.sensor.empty())
            item["sensor"] = t.sensor;
        if (!t.boosts.empty())
            item["boosts"] = named_calls_json(t.boosts);
        out.push_back(std::move(item));
    }
    return out;
}
int whole_number(const Json &j, int low, int high)
{
    return j.is_number_integer() && j >= low && j <= high ? j.get<int>() : 0;
}
std::vector<RoomScene> room_scenes(const Json &j)
{
    std::vector<RoomScene> out;
    if (j.is_array())
        for (const auto &s : j)
        {
            RoomScene scene{text(s, "name"),
                            text(s, "icon"),
                            whole_number(field(s, "brightness"), 1, 100),
                            whole_number(field(s, "kelvin"), 1000, 10000),
                            field(s, "off") == true,
                            service_call(s),
                            text(s, "entity")};
            if (!scene.name.empty() && out.size() < 6)
                out.push_back(std::move(scene));
        }
    return out;
}
Json room_scenes_json(const std::vector<RoomScene> &list)
{
    auto out = Json::array();
    for (const auto &s : list)
    {
        Json item{{"name", s.name}};
        if (!s.icon.empty())
            item["icon"] = s.icon;
        if (s.brightness)
            item["brightness"] = s.brightness;
        if (s.kelvin)
            item["kelvin"] = s.kelvin;
        if (s.off)
            item["off"] = true;
        put_call(item, s.call);
        if (!s.entity.empty())
            item["entity"] = s.entity;
        out.push_back(std::move(item));
    }
    return out;
}
HouseControls parse_house(const Json &j)
{
    HouseControls house;
    house.actions = named_calls(field(j, "actions"), 4);
    house.undo_lights = text(j, "undo_lights");
    const auto &modes = field(j, "modes");
    if (modes.is_array())
        for (const auto &m : modes)
        {
            HouseMode mode{text(m, "entity"),
                           text(m, "name"),
                           text(m, "icon"),
                           text(m, "about"),
                           service_call(field(m, "on")),
                           service_call(field(m, "off")),
                           field(m, "confirm_off") == true};
            if (!mode.entity.empty() && !mode.name.empty() && house.modes.size() < 8)
                house.modes.push_back(std::move(mode));
        }
    return house;
}
Json house_json(const HouseControls &house)
{
    auto modes = Json::array();
    for (const auto &m : house.modes)
    {
        Json item{{"entity", m.entity}, {"name", m.name}};
        if (!m.icon.empty())
            item["icon"] = m.icon;
        if (!m.about.empty())
            item["about"] = m.about;
        if (!m.on.empty())
            item["on"] = call_json(m.on);
        if (!m.off.empty())
            item["off"] = call_json(m.off);
        if (m.confirm_off)
            item["confirm_off"] = true;
        modes.push_back(std::move(item));
    }
    Json out{{"actions", named_calls_json(house.actions)}, {"modes", std::move(modes)}};
    if (!house.undo_lights.empty())
        out["undo_lights"] = house.undo_lights;
    return out;
}
Speakers parse_speakers(const Json &j)
{
    Speakers speakers;
    const auto &group = field(j, "group");
    speakers.group = text(group, "player");
    speakers.group_name = text(group, "name", speakers.group_name);
    const auto &rooms = field(j, "rooms");
    if (rooms.is_array())
        for (const auto &r : rooms)
        {
            Speaker speaker{text(r, "name"), text(r, "player"), text(r, "sleep_timer")};
            if (!speaker.player.empty() && speakers.rooms.size() < 8)
                speakers.rooms.push_back(std::move(speaker));
        }
    speakers.sleep_script = text(j, "sleep_script");
    if (!speakers.sleep_script.starts_with("script."))
        speakers.sleep_script.clear();
    const auto &minutes = field(j, "sleep_minutes");
    if (minutes.is_array())
    {
        speakers.sleep_minutes.clear();
        for (const auto &m : minutes)
            if (whole_number(m, 1, 720) && speakers.sleep_minutes.size() < 5)
                speakers.sleep_minutes.push_back(m.get<int>());
    }
    return speakers;
}
Json speakers_json(const Speakers &speakers)
{
    auto rooms = Json::array();
    for (const auto &r : speakers.rooms)
    {
        Json item{{"name", r.name}, {"player", r.player}};
        if (!r.sleep_timer.empty())
            item["sleep_timer"] = r.sleep_timer;
        rooms.push_back(std::move(item));
    }
    Json out{{"rooms", std::move(rooms)},
             {"sleep_script", speakers.sleep_script},
             {"sleep_minutes", speakers.sleep_minutes}};
    if (!speakers.group.empty())
        out["group"] = {{"name", speakers.group_name}, {"player", speakers.group}};
    return out;
}
RoomPage parse_room(const Json &r)
{
    RoomPage page{text(r, "name"),
                  text(r, "icon"),
                  text(r, "media"),
                  things(field(r, "tiles")),
                  things(field(r, "actions")),
                  text(r, "photo"),
                  text(r, "light"),
                  room_scenes(field(r, "scenes"))};
    if (page.actions.size() > 6)
        page.actions.resize(6);
    return page;
}
Json room_json(const RoomPage &r)
{
    Json room{{"name", r.name},
              {"icon", r.icon},
              {"media", r.media},
              {"tiles", things_json(r.tiles)},
              {"actions", things_json(r.actions)}};
    if (!r.photo.empty())
        room["photo"] = r.photo;
    if (!r.light.empty())
        room["light"] = r.light;
    if (!r.scenes.empty())
        room["scenes"] = room_scenes_json(r.scenes);
    return room;
}
// What can go behind the pages; anything else is the sky.
bool valid_background(const std::string &value)
{
    if (value == "sky" || value == "photos" || value == "room" || value.starts_with("/") ||
        value.starts_with("http://"))
        return true;
    return value.find('.') != std::string::npos && value.find('/') == std::string::npos;
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
        {"clock", clock},
        {"photos", photos},
        {"photo_seconds", photo_seconds},
        {"background", background},
        {"mood", mood},
        {"pipeline", pipeline},
        {"settings_entity", settings_entity},
        {"reduced_motion", reduced_motion},
        {"vibration", vibration}};
    if (!categories.empty())
        j["categories"] = categories_json(categories);
    if (!rooms.empty())
    {
        auto list = Json::array();
        for (const auto &r : rooms)
            list.push_back(room_json(r));
        j["rooms"] = std::move(list);
    }
    if (!house.empty())
        j["house"] = house_json(house);
    if (!speakers.rooms.empty() || !speakers.group.empty() || !speakers.sleep_script.empty())
        j["speakers"] = speakers_json(speakers);
    if (!notices.entity.empty())
        j["notices"] = {{"entity", notices.entity}, {"dismiss_event", notices.dismiss_event}};
    if (!doorbell.empty())
        j["doorbell"] = {
            {"trigger", doorbell.trigger}, {"camera", doorbell.camera}, {"name", doorbell.name}};
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
    c.view_assist = text(j, "view_assist");
    if (c.view_assist.empty())
        c.view_assist = text(j, "view_assist_sensor");
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
    // rooms, or 1.0's single room_page.
    const auto &list = field(j, "rooms");
    if (list.is_array())
    {
        for (const auto &r : list)
            if (auto page = parse_room(r); !page.empty() && c.rooms.size() < 6)
                c.rooms.push_back(std::move(page));
    }
    else if (auto page = parse_room(field(j, "room_page")); !page.empty())
        c.rooms.push_back(std::move(page));
    c.house = parse_house(field(j, "house"));
    c.speakers = parse_speakers(field(j, "speakers"));
    const auto &clock = field(j, "clock");
    if (clock == "12h" || clock == "24h")
        c.clock = clock.get<std::string>();
    const auto &bell = field(j, "doorbell");
    c.doorbell = {text(bell, "trigger"), text(bell, "camera"), text(bell, "name")};
    c.photos = text(j, "photos");
    if (!c.photos.starts_with("media-source://"))
        c.photos.clear();
    const auto &seconds = field(j, "photo_seconds");
    if (seconds.is_number_integer() && seconds >= 5 && seconds <= 3600)
        c.photo_seconds = seconds.get<int>();
    if (const auto background = text(j, "background"); valid_background(background))
        c.background = background;
    c.pipeline = text(j, "pipeline");
    c.mood = field(j, "mood") != false;
    const auto &notices = field(j, "notices");
    c.notices = {text(notices, "entity"), text(notices, "dismiss_event")};
    if (field(j, "settings_entity").is_string())
        c.settings_entity = text(j, "settings_entity");
    c.reduced_motion = field(j, "reduced_motion") == true;
    c.vibration = field(j, "vibration") == true;
    return c;
}
Config merged(const Config &base, const Json &over, bool connection)
{
    auto j = base.json();
    if (over.is_object())
        for (auto it = over.begin(); it != over.end(); ++it)
            if (connection || (it.key() != "host" && it.key() != "port" && it.key() != "token" &&
                               it.key() != "settings_entity"))
                j[it.key()] = it.value();
    // A file or entity that says room_page means its one room, over the rooms saved before.
    if (over.is_object() && over.contains("room_page") && !over.contains("rooms"))
        j.erase("rooms");
    return Config::parse(j);
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
