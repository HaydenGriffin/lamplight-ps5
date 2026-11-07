// Lamplight for PS5 - Setup: the connection, what each page shows, and the PS5 keyboard.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include <algorithm>
#include <cstdlib>

namespace lamplight
{
using hui::ui::ListItem;
namespace
{
enum Row
{
    address,
    port,
    token,
    connect,
    mood_row,
    background_row,
    folder_row,
    interval_row,
    weather_row,
    calendars_row,
    view_assist_row,
    appliances_row,
    agent_row,
    wake_row,
    room_row,
    ssid_row,
    password_row,
    visitors_row,
    clock_row,
    motion_row,
    vibration_row,
    forget_row,
    close_row
};
enum Keyboard
{
    host_kb = 1,
    token_kb,
    port_kb,
    wake_kb,
    ask_kb,
    room_kb,
    ssid_kb,
    password_kb
};
bool chosen(const std::vector<Choice> &list, const std::string &id)
{
    return std::any_of(list.begin(), list.end(), [&](const Choice &c) { return c.entity == id; });
}
} // namespace

std::vector<CatalogEntry> Application::pick_options() const
{
    std::vector<CatalogEntry> out;
    for (const auto &e : snapshot_.catalog)
    {
        const auto domain = domain_of(e.id);
        const bool fits = pick_ == Pick::weather       ? domain == "weather"
                          : pick_ == Pick::calendars   ? domain == "calendar"
                          : pick_ == Pick::view_assist ? e.has_timers
                          : pick_ == Pick::appliances  ? domain == "timer"
                          : pick_ == Pick::agent       ? domain == "conversation"
                          : pick_ == Pick::photos      ? false
                                                  : domain == "input_boolean" || domain == "switch";
        if (fits)
            out.push_back(e);
    }
    return out;
}
void Application::rebuild_list()
{
    std::vector<ListItem> rows;
    auto row = [&](std::string title, std::string sub = "", std::string value = "",
                   bool disabled = false, bool opens_list = false)
    {
        ListItem r;
        r.chevron = opens_list;
        r.title = std::move(title);
        r.subtitle = std::move(sub);
        r.value = std::move(value);
        r.disabled = disabled;
        rows.push_back(std::move(r));
    };
    auto name = [&](const std::string &id, const char *none)
    {
        if (id.empty())
            return std::string(none);
        for (const auto &e : snapshot_.catalog)
            if (e.id == id)
                return e.name;
        return id;
    };
    auto count = [](std::size_t n, const char *one, const char *many)
    { return n == 0 ? std::string("None") : std::to_string(n) + " " + (n == 1 ? one : many); };
    if (page_ == Page::setup)
    {
        row("Home Assistant address", "Numeric IPv4 on your local network",
            config_.host.empty() ? "Not set" : config_.host);
        row("Port", "Home Assistant's local HTTP port", std::to_string(config_.port));
        row("Access token", "Long-lived token; hidden while you type",
            config_.token.empty() ? "Not set" : "Set");
        row("Connect / retry", "Apply the address and token");
        row("Resting clock", "Stars, clouds and rain behind it, or plain",
            config_.mood ? "Sky" : "Plain");
        row("Background", "Behind the clock and the pages",
            config_.background == "sky"      ? "Sky"
            : config_.background == "photos" ? "Photos"
            : config_.background == "room"   ? "Room photo"
                                             : "Picture");
        row("Photo folder", "From Home Assistant's media: the slideshow and the resting clock",
            config_.photos.empty() ? "None" : photo_folder_name(config_.photos), !ready(), true);
        row("Photo every", "How long each photo stays",
            config_.photo_seconds < 60 ? std::to_string(config_.photo_seconds) + " s"
                                       : std::to_string(config_.photo_seconds / 60) + " min");
        row("Weather", "The forecast for Weather and the clock", name(config_.weather, "None"),
            !ready(), true);
        row("Calendars", "Today and tomorrow on Agenda",
            count(config_.calendars.size(), "calendar", "calendars"), !ready(), true);
        row("View Assist device", "Its timers, and its assistant's answers",
            name(config_.view_assist, "None"), !ready(), true);
        row("Appliance timers", "Washing machine, dryer: timers with finishes_at",
            count(config_.appliances.size(), "timer", "timers"), !ready(), true);
        row("Assistant", "The voice assistant Help asks", name(config_.agent, "Default"), !ready(),
            true);
        row("Wake phrase", "Shown before each example", config_.wake_phrase);
        row("Room", "Fills {room} in Help's examples",
            config_.room.empty() ? "Not set" : config_.room);
        row("Guest Wi-Fi network", "The name guests join",
            config_.wifi_ssid.empty() ? "Not set" : config_.wifi_ssid);
        row("Guest Wi-Fi password", "Only ever shown inside the QR code",
            config_.wifi_password.empty() ? "None (open)" : "Set");
        row("Visitors switch", "While it's on, Help's examples run nothing",
            name(config_.read_only, "None"), !ready(), true);
        row("Clock", "12 or 24 hours; Automatic follows Home Assistant's country",
            config_.clock == "12h"   ? "12-hour"
            : config_.clock == "24h" ? "24-hour"
                                     : "Automatic");
        row("Reduce motion", "No pulsing dot, no moving focus",
            config_.reduced_motion ? "On" : "Off");
        row("Controller vibration", "A short rumble on each move",
            config_.vibration ? "On" : "Off");
        row("Disconnect and forget token", "Keep the other settings on this PS5");
        row("Close Lamplight", "Return to the PS5 home screen");
    }
    else if (page_ == Page::pick && pick_ == Pick::photos)
    {
        pick_ids_.clear();
        const bool top = folder_path_.size() <= 1;
        row(top ? "No photos" : "Use this folder",
            top ? "The resting clock draws the sky"
                : std::to_string(folder_pictures_) + " pictures here, and any in its folders",
            config_.photos == folder_path_.back() ? "Selected" : "",
            folder_loading_ || folder_failed_);
        pick_ids_.push_back("");
        if (folder_loading_)
            row("Reading\xE2\x80\xA6", "", "", true);
        for (const auto &[id, title] : folder_items_)
        {
            row(title, "", "", false, true);
            pick_ids_.push_back(id);
        }
    }
    else if (page_ == Page::pick)
    {
        pick_ids_.clear();
        const bool optional = pick_ == Pick::view_assist || pick_ == Pick::read_only;
        if (optional)
            pick_ids_.push_back("");
        if (optional)
            row("None", pick_ == Pick::view_assist ? "Appliance timers only" : "Help always runs",
                (pick_ == Pick::view_assist ? config_.view_assist : config_.read_only).empty()
                    ? "Selected"
                    : "");
        for (const auto &e : pick_options())
        {
            std::string value;
            if (pick_ == Pick::calendars)
                value = chosen(config_.calendars, e.id) ? "On" : "Off";
            else if (pick_ == Pick::appliances)
                value = chosen(config_.appliances, e.id) ? "On" : "Off";
            else
            {
                const auto &current = pick_ == Pick::weather       ? config_.weather
                                      : pick_ == Pick::view_assist ? config_.view_assist
                                      : pick_ == Pick::agent       ? config_.agent
                                                                   : config_.read_only;
                value = current == e.id ? "Selected" : "";
            }
            row(e.name, e.id, value);
            pick_ids_.push_back(e.id);
        }
    }
    else if (page_ == Page::quit)
    {
        row("Keep Lamplight open");
        row("Close Lamplight", "Return to the PS5 home screen");
    }
    const int focus = list_.focus();
    list_.set_items(std::move(rows));
    list_.set_focus(focus);
}
void Application::refresh_lists()
{
    if ((page_ != Page::setup && page_ != Page::pick) ||
        (page_ == Page::pick && pick_ == Pick::photos))
        return;
    std::size_t catalog = std::hash<std::string>{}(
        [&]
        {
            std::string all;
            for (const auto &e : snapshot_.catalog)
                all += e.id + "|" + e.name + ",";
            return all;
        }());
    if (snapshot_.ready == lists_ready_ && catalog == lists_catalog_)
        return;
    lists_ready_ = snapshot_.ready;
    lists_catalog_ = catalog;
    const int focus = list_.focus();
    const std::string focused =
        page_ == Page::pick && focus >= 0 && focus < static_cast<int>(pick_ids_.size())
            ? pick_ids_[static_cast<std::size_t>(focus)]
            : "";
    rebuild_list();
    if (page_ == Page::pick)
        for (std::size_t i = 0; i < pick_ids_.size(); ++i)
            if (pick_ids_[i] == focused)
                list_.set_focus(static_cast<int>(i));
}
int Application::pick_current_row() const
{
    const std::string *current = pick_ == Pick::weather       ? &config_.weather
                                 : pick_ == Pick::view_assist ? &config_.view_assist
                                 : pick_ == Pick::agent       ? &config_.agent
                                 : pick_ == Pick::read_only   ? &config_.read_only
                                                              : nullptr;
    if (!current)
        return 0;
    const auto it = std::find(pick_ids_.begin(), pick_ids_.end(), *current);
    return it == pick_ids_.end() ? 0 : static_cast<int>(it - pick_ids_.begin());
}
void Application::open_pick(Pick pick)
{
    if (!ready())
        return;
    pick_ = pick;
    if (pick == Pick::photos)
    {
        // The way down exists before the list is drawn from it.
        folder_path_ = {""};
        folder_items_.clear();
        open(Page::pick);
        browse_folder();
        return;
    }
    if (refresh_catalog)
        refresh_catalog();
    open(Page::pick);
}
void Application::activate_setup(int index)
{
    switch (index)
    {
    case address:
        request_keyboard(host_kb);
        break;
    case port:
        request_keyboard(port_kb);
        break;
    case token:
        request_keyboard(token_kb);
        break;
    case connect:
        if (reconnect)
            reconnect(config_, watched());
        toast("Connecting…");
        break;
    case weather_row:
        open_pick(Pick::weather);
        break;
    case calendars_row:
        open_pick(Pick::calendars);
        break;
    case view_assist_row:
        open_pick(Pick::view_assist);
        break;
    case appliances_row:
        open_pick(Pick::appliances);
        break;
    case agent_row:
        open_pick(Pick::agent);
        break;
    case wake_row:
        request_keyboard(wake_kb);
        break;
    case room_row:
        request_keyboard(room_kb);
        break;
    case ssid_row:
        request_keyboard(ssid_kb);
        break;
    case password_row:
        request_keyboard(password_kb);
        break;
    case visitors_row:
        open_pick(Pick::read_only);
        break;
    case clock_row:
        config_.clock = config_.clock == "auto" ? "12h" : config_.clock == "12h" ? "24h" : "auto";
        persist();
        break;
    case mood_row:
        config_.mood = !config_.mood;
        persist();
        break;
    case background_row:
    {
        // Sky, then the folder's photos, then the View Assist device's room photo, when each can
        // be had.
        const bool has_photos = !config_.photos.empty();
        const bool has_room =
            !config_.view_assist.empty() &&
            !text(snapshot_.states.get(config_.view_assist).attributes, "background").empty();
        const auto &now_on = config_.background;
        config_.background = now_on == "sky" && has_photos                         ? "photos"
                             : (now_on == "sky" || now_on == "photos") && has_room ? "room"
                                                                                   : "sky";
        if (config_.background == "sky" && now_on == "sky" && !has_photos)
            toast("Choose a photo folder first", false, 4000);
        apply_settings();
        break;
    }
    case folder_row:
        open_pick(Pick::photos);
        return;
    case interval_row:
    {
        static constexpr int kChoices[] = {10, 20, 30, 60, 120, 300, 900};
        int at = 0;
        while (at < 6 && kChoices[at] < config_.photo_seconds)
            ++at;
        config_.photo_seconds = kChoices[(at + 1) % 7];
        persist();
        break;
    }
    case motion_row:
        config_.reduced_motion = !config_.reduced_motion;
        apply_settings();
        break;
    case vibration_row:
        config_.vibration = !config_.vibration;
        persist();
        break;
    case forget_row:
        config_.token.clear();
        persist();
        if (reconnect)
            reconnect(config_, watched());
        toast("Disconnected. The token is removed from this PS5.");
        break;
    case close_row:
        open(Page::quit);
        return;
    }
    rebuild_list();
}
void Application::activate_pick(int index)
{
    if (pick_ == Pick::photos)
    {
        activate_folder(index);
        return;
    }
    if (index < 0 || index >= static_cast<int>(pick_ids_.size()))
        return;
    const std::string id = pick_ids_[static_cast<std::size_t>(index)];
    auto toggle = [&](std::vector<Choice> &list)
    {
        auto it =
            std::find_if(list.begin(), list.end(), [&](const Choice &c) { return c.entity == id; });
        if (it != list.end())
        {
            set_aside_[id] = *it;
            list.erase(it);
        }
        else if (auto kept = set_aside_.find(id); kept != set_aside_.end())
            list.push_back(kept->second);
        else
            list.push_back({id, "", ""});
    };
    switch (pick_)
    {
    case Pick::weather:
        if (config_.weather != id)
        {
            config_.weather = id;
            forecast_ = {};
            forecast_failed_at_ = 0;
        }
        break;
    case Pick::calendars:
        toggle(config_.calendars);
        break;
    case Pick::view_assist:
        config_.view_assist = id;
        break;
    case Pick::appliances:
        toggle(config_.appliances);
        break;
    case Pick::agent:
        if (config_.agent != id)
            conversation_.clear();
        config_.agent = id;
        break;
    case Pick::read_only:
        config_.read_only = id;
        break;
    case Pick::photos:
        break;
    }
    apply_settings();
    // Lists of several stay open to choose more; a single choice goes back to Setup.
    if (pick_ == Pick::calendars || pick_ == Pick::appliances)
        rebuild_list();
    else
        open(Page::setup);
}
void Application::request_keyboard(int kind)
{
    keyboard_kind_ = kind;
    hui::ps5::KeyboardOptions k;
    k.max_length = 128;
    switch (kind)
    {
    case host_kb:
        k.title = "Home Assistant IPv4 address";
        k.initial = config_.host;
        k.max_length = 64;
        k.kind = hui::ps5::KeyboardKind::url;
        break;
    case token_kb:
        k.title = "Home Assistant access token";
        k.secret = true;
        k.max_length = 2048;
        break;
    case port_kb:
        k.title = "Home Assistant port";
        k.initial = std::to_string(config_.port);
        k.max_length = 5;
        k.kind = hui::ps5::KeyboardKind::number;
        break;
    case wake_kb:
        k.title = "Wake phrase";
        k.initial = config_.wake_phrase;
        break;
    case ask_kb:
        k.title = "Ask " + (config_.wake_phrase.empty() ? std::string("the assistant")
                                                        : config_.wake_phrase);
        k.max_length = 256;
        break;
    case room_kb:
        k.title = "This room's name, as you say it";
        k.initial = config_.room;
        break;
    case ssid_kb:
        k.title = "Guest Wi-Fi network name";
        k.initial = config_.wifi_ssid;
        k.max_length = 32;
        break;
    case password_kb:
        k.title = "Guest Wi-Fi password (empty for an open network)";
        k.secret = true;
        k.max_length = 63;
        break;
    default:
        return;
    }
    keyboard_ = std::move(k);
}
std::optional<hui::ps5::KeyboardOptions> Application::take_keyboard()
{
    auto k = std::move(keyboard_);
    keyboard_.reset();
    return k;
}
void Application::keyboard_result(const hui::ps5::KeyboardOutcome &result)
{
    if (!result.accepted)
        return;
    switch (keyboard_kind_)
    {
    case host_kb:
        if (!valid_ipv4(result.text))
        {
            toast("Use a numeric IPv4 address, such as 192.168.1.10", true, 5000);
            return;
        }
        config_.host = result.text;
        persist();
        toast("Choose Connect / retry to use it");
        break;
    case token_kb:
        if (result.text.empty() || result.text.find_first_of("\r\n") != std::string::npos)
        {
            toast("The token cannot be empty", true, 5000);
            return;
        }
        config_.token = result.text;
        persist();
        toast("Choose Connect / retry to use it");
        break;
    case port_kb:
    {
        char *end = nullptr;
        const long value = std::strtol(result.text.c_str(), &end, 10);
        if (value < 1 || value > 65535 || !end || *end)
        {
            toast("The port is a number from 1 to 65535", true, 5000);
            return;
        }
        config_.port = static_cast<int>(value);
        persist();
        toast("Choose Connect / retry to use it");
        break;
    }
    case wake_kb:
        config_.wake_phrase = result.text;
        persist();
        break;
    case ask_kb:
        if (!result.text.empty())
            ask_assistant(result.text, config_.agent, true);
        return;
    case room_kb:
        config_.room = result.text;
        persist();
        break;
    case ssid_kb:
        config_.wifi_ssid = result.text;
        apply_settings();
        break;
    case password_kb:
        config_.wifi_password = result.text;
        apply_settings();
        break;
    default:
        return;
    }
    rebuild_list();
}
} // namespace lamplight
