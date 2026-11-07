// Lamplight for PS5 - Background payload: Home Assistant's notifications on the PS5, over any game;
// the console's game, temperatures, free space and uptime for Home Assistant; and its commands.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Loaded at boot by the autoloader (or sent to an ELF loader). It reads
// /data/lamplight/notify.json ({"host", "port", "token"}), connects to Home Assistant's websocket,
// subscribes to the event lamplight_ps5_notify ({"title", "message"}) and shows each one as a
// system notification, and to lamplight_ps5_command ({"command": "launch", "title_id"},
// {"command": "lamplight"} or {"command": "rest"}). It fires lamplight_ps5_status once a minute
// and when the game in front changes. It opens no listening port to the network: the one it
// binds, on 127.0.0.1, only stops a second copy starting.
#include "console.hpp"
#include "afterglow/ma_socket.hpp"
#include "lamplight/clock.hpp"
#include "lamplight/config.hpp"
#include "lamplight/json.hpp"
#include "platform/ps5/system.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <cstdio>
#include <netinet/in.h>
#include <string>
#include <sys/mount.h>
#include <sys/socket.h>
#include <unistd.h>

extern "C"
{
    int sceNotificationSend(int user, bool logged, const char *payload);
    int sceKernelGetCpuTemperature(int *celsius);
    int sceKernelGetSocSensorTemperature(int sensor, int *celsius);
}

namespace
{
using hui::sys::monotonic_us;
using lamplight::field;
using lamplight::Json;
using lamplight::text;
constexpr int kSystemUser = 0xFE;
constexpr const char *kConfig = "/data/lamplight/notify.json";
// Where the system keeps a registered title's icon (it shows nothing for the install folder's
// copy).
constexpr const char *kIcon = "/user/appmeta/PPSA99484/icon0.png";

struct Settings
{
    std::string host, token;
    int port = 8123;
};
bool read_settings(Settings &out)
{
    std::FILE *file = std::fopen(kConfig, "rb");
    if (!file)
        return false;
    std::string data(65536, '\0');
    data.resize(std::fread(data.data(), 1, data.size(), file));
    std::fclose(file);
    const auto j = Json::parse(data, nullptr, false);
    out.host = text(j, "host");
    out.token = text(j, "token");
    if (field(j, "port").is_number_integer())
        out.port = field(j, "port").get<int>();
    return lamplight::valid_ipv4(out.host) && !out.token.empty();
}
// The PS5's toast: the title large, the message under it, Lamplight's icon.
void notify(const std::string &title, const std::string &message)
{
    const auto now = lamplight::wall_clock();
    Json view{{"icon", {{"type", "Url"}, {"parameters", {{"url", kIcon}}}}},
              {"message", {{"body", title}}}};
    if (!message.empty())
        view["subMessage"] = {{"body", message}};
    Json toast{{"rawData",
                {{"viewTemplateType", "InteractiveToastTemplateB"},
                 {"channelType", "Downloads"},
                 {"useCaseId", "IDC"},
                 {"toastOverwriteType", "No"},
                 {"isImmediate", true},
                 {"priority", 100},
                 {"viewData", view},
                 {"platformViews",
                  {{"previewDisabled",
                    {{"viewData",
                      {{"icon", {{"type", "Predefined"}, {"parameters", {{"icon", "download"}}}}},
                       {"message", {{"body", title}}}}}}}}}}},
               {"createdDateTime", lamplight::iso_local(now.utc_ms, lamplight::Zone{})},
               {"localNotificationId", std::to_string(now.utc_ms % 1000000000)}};
    const int rc = sceNotificationSend(kSystemUser, true, toast.dump().c_str());
    hui::sys::log("[Lamplight notify] \"%s\" rc=%d", title.c_str(), rc);
}
Json status(const lamplight::console::Game &game)
{
    Json out{{"uptime_s", monotonic_us() / 1000000}};
    out["game"] = game.title_id.empty()
                      ? Json()
                      : Json{{"title_id", game.title_id},
                             {"name", game.name.empty() ? game.title_id : game.name}};
    int celsius = 0;
    if (sceKernelGetSocSensorTemperature(0, &celsius) == 0)
        out["soc_temperature"] = celsius;
    if (sceKernelGetCpuTemperature(&celsius) == 0)
        out["cpu_temperature"] = celsius;
    struct statfs fs
    {
    };
    if (statfs("/user", &fs) == 0)
        out["free_gb"] = static_cast<long long>(fs.f_bavail * fs.f_bsize / 1000000000ULL);
    return out;
}
// One copy only: a second finds the port taken and leaves.
bool single_instance()
{
    const int s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(48484);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return s >= 0 && bind(s, reinterpret_cast<sockaddr *>(&address), sizeof address) == 0 &&
           listen(s, 1) == 0;
}
// A command from Home Assistant: open a title (or Lamplight), or rest.
void command(const Json &data)
{
    const auto what = text(data, "command");
    if (what == "launch" || what == "lamplight")
    {
        const auto id = what == "lamplight" ? std::string("PPSA99484") : text(data, "title_id");
        const auto name = lamplight::console::title_name(id);
        switch (lamplight::console::launch(id))
        {
        case lamplight::console::Launch::opened:
            break;
        case lamplight::console::Launch::busy:
            notify("Couldn't open " + (name.empty() ? id : name), "Close the game in front first");
            break;
        case lamplight::console::Launch::failed:
            notify("Couldn't open " + (name.empty() ? id : name), "The PS5 refused");
            break;
        }
    }
    else if (what == "rest")
    {
        if (!lamplight::console::rest())
            notify("Couldn't go to rest mode", "This PS5 doesn't let Lamplight's payload do it");
    }
    else
        hui::sys::log("[Lamplight notify] unknown command \"%s\"", what.c_str());
}
// One connection, until it drops. Returns false when the token was refused (no point retrying).
bool session(const Settings &settings, std::atomic<bool> &stopping)
{
    hui::afterglow::ma_socket::Socket socket(stopping);
    int next = 1, subscription = 0, commands = 0, ping_id = 0;
    bool authorized = false, refused = false;
    std::int64_t last_ping = monotonic_us(), ping_deadline = 0, next_status = 0, next_game = 0;
    lamplight::console::Game game;
    auto send = [&](Json j)
    {
        j["id"] = next++;
        return socket.send_text(j.dump()) ? next - 1 : 0;
    };
    socket.handler = [&](int opcode, const std::uint8_t *bytes, std::size_t size)
    {
        if (opcode != 1)
            return true;
        const auto j = Json::parse(bytes, bytes + size, nullptr, false);
        const auto type = text(j, "type");
        if (type == "auth_required")
            return socket.send_text(
                Json{{"type", "auth"}, {"access_token", settings.token}}.dump());
        if (type == "auth_invalid")
        {
            refused = true;
            hui::sys::log("[Lamplight notify] Home Assistant refused the token");
            return false;
        }
        if (type == "auth_ok")
        {
            authorized = true;
            subscription =
                send({{"type", "subscribe_events"}, {"event_type", "lamplight_ps5_notify"}});
            commands =
                send({{"type", "subscribe_events"}, {"event_type", "lamplight_ps5_command"}});
            hui::sys::log("[Lamplight notify] connected to %s", settings.host.c_str());
            return subscription != 0 && commands != 0;
        }
        if (type == "pong")
            ping_deadline = 0;
        // Home Assistant lets only an administrator subscribe to an event of its own: anyone else
        // is refused here, and would otherwise sit connected receiving nothing.
        if (type == "result" && (field(j, "id") == subscription || field(j, "id") == commands) &&
            field(j, "success") != true)
        {
            refused = true;
            hui::sys::log("[Lamplight notify] Home Assistant refused the subscription: the token "
                          "must be an administrator's");
            return false;
        }
        if (type == "event" && field(j, "id") == commands)
            command(field(field(j, "event"), "data"));
        if (type == "event" && field(j, "id") == subscription)
        {
            const auto &data = field(field(j, "event"), "data");
            const auto title = text(data, "title", text(data, "message"));
            if (!title.empty())
                notify(title.substr(0, 200),
                       text(data, "title").empty() ? "" : text(data, "message").substr(0, 400));
        }
        return true;
    };
    if (!socket.open(settings.host, static_cast<std::uint16_t>(settings.port), "/api/websocket"))
        return true;
    while (!stopping)
    {
        if (!socket.tick())
            break;
        const auto now = monotonic_us();
        if (ping_deadline && now > ping_deadline)
            break;
        if (authorized && now - last_ping > 30000000)
        {
            ping_id = send({{"type", "ping"}});
            last_ping = now;
            ping_deadline = now + 10000000;
            if (!ping_id)
                break;
        }
        // The game in front, every five seconds: a new one is news at once.
        if (authorized && now >= next_game)
        {
            auto in_front = lamplight::console::running_game();
            if (in_front.title_id != game.title_id)
                next_status = 0;
            game = std::move(in_front);
            next_game = now + 5000000;
        }
        // The status, once a minute and when the game changes (fire_event, an administrator's).
        if (authorized && now >= next_status)
        {
            send({{"type", "fire_event"},
                  {"event_type", "lamplight_ps5_status"},
                  {"event_data", status(game)}});
            next_status = now + 60000000;
        }
        hui::sys::sleep_us(50000);
    }
    socket.close();
    return !refused;
}
} // namespace

int main()
{
    if (!single_instance())
    {
        hui::sys::log("[Lamplight notify] already running");
        return 0;
    }
    std::atomic<bool> stopping{false};
    int attempts = 0;
    for (;;)
    {
        Settings settings;
        if (!read_settings(settings))
        {
            // No settings yet (or unreadable): look again in a minute, so copying the file is
            // enough to start it.
            hui::sys::sleep_us(60000000);
            continue;
        }
        const auto started = monotonic_us();
        if (!session(settings, stopping))
        {
            // A refused token (or a user who may not subscribe) waits five minutes rather than
            // knocking every few seconds.
            hui::sys::sleep_us(300000000);
            continue;
        }
        attempts = monotonic_us() - started > 60000000 ? 0 : attempts + 1;
        const int seconds = attempts > 5 ? 60 : 1 << attempts;
        hui::sys::sleep_us(static_cast<std::uint32_t>(seconds) * 1000000u);
    }
}
