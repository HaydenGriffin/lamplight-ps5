// Lamplight for PS5 - Reconnecting Home Assistant session: states, catalogue and requests.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/client.hpp"
#include "lamplight/clock.hpp"
#include "afterglow/ma_socket.hpp"
#include "platform/ps5/system.hpp"
#include <algorithm>
#include <map>

namespace lamplight
{
using hui::sys::monotonic_us;
bool allowed(const Json &m, const Config &config)
{
    const auto type = text(m, "type");
    if (type == "conversation/process")
        return !text(m, "text").empty();
    if (type != "call_service")
        return false;
    const auto call = text(m, "domain") + "." + text(m, "service");
    if (call == "weather.get_forecasts" || call == "calendar.get_events" ||
        call == "view_assist.set_timer" || call == "view_assist.cancel_timer" ||
        call == "view_assist.set_state")
        return true;
    const auto entity = text(field(m, "target"), "entity_id");
    std::vector<std::string> room;
    if (!config.room_page.media.empty())
        room.push_back(config.room_page.media);
    for (const auto *list : {&config.room_page.tiles, &config.room_page.actions})
        for (const auto &t : *list)
            room.push_back(t.entity);
    for (const auto &id : room)
        if (id == entity)
        {
            if (domain_of(entity) != text(m, "domain"))
                return false;
            const auto name = text(m, "service");
            return name == "toggle" || name == "turn_on" || name == "media_play_pause" ||
                   name == "media_next_track";
        }
    return false;
}
Request service(const std::string &domain, const std::string &name, Json data,
                const std::string &entity, bool response)
{
    Json m{{"type", "call_service"},
           {"domain", domain},
           {"service", name},
           {"service_data", std::move(data)}};
    if (!entity.empty())
        m["target"] = {{"entity_id", entity}};
    if (response)
        m["return_response"] = true;
    return {std::move(m)};
}
Request ask(const std::string &text, const std::string &agent, const std::string &conversation)
{
    Json m{{"type", "conversation/process"}, {"text", text}};
    if (!agent.empty())
        m["agent_id"] = agent;
    if (!conversation.empty())
        m["conversation_id"] = conversation;
    // The assistant can take a while (a language model, a slow integration): the Help card
    // waits 25 seconds.
    return {std::move(m), 25000000};
}
Client::~Client()
{
    stop();
}
Snapshot Client::snapshot() const
{
    std::lock_guard lock(mutex_);
    return snapshot_;
}
bool Client::start(const Config &config, std::set<std::string> watched)
{
    stop();
    config_ = config;
    {
        std::lock_guard lock(mutex_);
        auto revision = snapshot_.revision + 1;
        auto generation = snapshot_.generation;
        snapshot_ = Snapshot{};
        snapshot_.revision = revision;
        snapshot_.generation = generation;
        snapshot_.status = config.configured() ? "Connecting to Home Assistant"
                                               : "Enter a numeric IPv4 address, port and token";
        watched_ = std::move(watched);
        watch_changed_ = false;
        catalog_due_ = false;
    }
    if (!config.configured())
        return false;
    stopping_ = false;
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0)
        return false;
    int rc = pthread_attr_setstacksize(&attr, 1024 * 1024);
    if (!rc)
        rc = pthread_create(
            &thread_, &attr,
            [](void *p) -> void *
            {
                static_cast<Client *>(p)->run();
                return nullptr;
            },
            this);
    pthread_attr_destroy(&attr);
    started_ = rc == 0;
    if (!started_)
    {
        stopping_ = true;
        std::lock_guard lock(mutex_);
        snapshot_.status = "Could not start the network worker. Restart Lamplight.";
    }
    return started_;
}
void Client::stop()
{
    stopping_ = true;
    if (started_)
        pthread_join(thread_, nullptr);
    started_ = false;
    std::lock_guard lock(mutex_);
    queue_.clear();
    snapshot_.ready = false;
    snapshot_.connected = false;
}
std::uint64_t Client::send(Request r)
{
    std::lock_guard lock(mutex_);
    if (!snapshot_.ready || queue_.size() >= 16 || !allowed(r.message, config_))
        return 0;
    const auto ticket = ++ticket_;
    queue_.push_back({std::move(r), ticket, monotonic_us() + 5000000, snapshot_.generation});
    return ticket;
}
void Client::watch(std::set<std::string> watched)
{
    std::lock_guard lock(mutex_);
    if (watched == watched_)
        return;
    watched_ = std::move(watched);
    watch_changed_ = true;
}
void Client::refresh_catalog()
{
    std::lock_guard lock(mutex_);
    catalog_due_ = true;
}
void Client::run()
{
    int attempts = 0;
    while (!stopping_)
    {
        Snapshot state;
        {
            std::lock_guard lock(mutex_);
            state.generation = snapshot_.generation + 1;
            state.revision = snapshot_.revision;
            state.clock_skew_ms = snapshot_.clock_skew_ms;
            state.clock_known = snapshot_.clock_known;
            watch_changed_ = false;
            catalog_due_ = false;
        }
        auto publish = [&]
        {
            std::lock_guard lock(mutex_);
            state.revision++;
            snapshot_ = state;
        };
        auto result = [&](std::uint64_t ticket, bool ok, std::string message, Json response = {})
        {
            state.results.push_back({ticket, ok, std::move(message), std::move(response)});
            if (state.results.size() > 64)
                state.results.erase(state.results.begin());
        };
        state.status = "Connecting to Home Assistant";
        publish();
        hui::afterglow::ma_socket::Socket socket(stopping_);
        int next = 1, catalog_id = 0, subscription = 0, ping_id = 0;
        bool authorized = false, invalid_auth = false, subscribed = false, have_initial = false;
        std::set<std::string> subscribed_ids;
        struct Pending
        {
            std::uint64_t ticket;
            std::int64_t expires;
        };
        std::map<int, Pending> pending;
        auto deadline = monotonic_us() + 15000000, last_ping = monotonic_us();
        std::int64_t ping_deadline = 0;
        auto send = [&](Json j)
        {
            j["id"] = next++;
            return socket.send_text(j.dump()) ? next - 1 : 0;
        };
        auto subscribe = [&]
        {
            std::set<std::string> ids;
            {
                std::lock_guard lock(mutex_);
                ids = watched_;
                watch_changed_ = false;
            }
            // The sun says whether it is day or night for the sky.
            ids.insert("sun.sun");
            if (subscription)
                send({{"type", "unsubscribe_events"}, {"subscription", subscription}});
            subscribed_ids = ids;
            state.states.clear();
            subscribed = false;
            have_initial = false;
            subscription = send({{"type", "subscribe_entities"}, {"entity_ids", ids}});
            if (!subscription)
                return false;
            if (!state.ready)
                deadline = monotonic_us() + 15000000;
            return true;
        };
        socket.handler = [&](int opcode, const std::uint8_t *bytes, std::size_t size)
        {
            if (opcode != 1)
                return true;
            auto j = Json::parse(bytes, bytes + size, nullptr, false);
            if (!j.is_object())
                return false;
            auto type = text(j, "type");
            int id = 0;
            const auto &idj = field(j, "id");
            if (idj.is_number_integer() && idj >= 0 && idj <= 2147483647)
                id = idj.get<int>();
            if (type == "auth_required")
                return socket.send_text(
                    Json{{"type", "auth"}, {"access_token", config_.token}}.dump());
            if (type == "auth_invalid")
            {
                invalid_auth = true;
                state.status = "Home Assistant rejected the token. Replace it in Setup.";
                return false;
            }
            if (type == "auth_ok")
            {
                authorized = true;
                state.connected = true;
                state.status = "Reading Home Assistant";
                catalog_id = send({{"type", "get_states"}});
                publish();
                return catalog_id != 0;
            }
            if (type == "pong" && id == ping_id)
            {
                ping_deadline = 0;
                return true;
            }
            if (type == "event")
            {
                if (id == subscription)
                {
                    state.states.apply(field(j, "event"));
                    have_initial = true;
                    // A new subscription (Setup changed the entities) keeps the session ready.
                    state.ready = state.ready || subscribed;
                    if (state.ready)
                        deadline = 0;
                    state.status = state.ready ? "Connected" : "Reading Home Assistant";
                    publish();
                }
                return true;
            }
            if (type != "result")
                return true;
            const bool ok = field(j, "success") == true;
            if (auto it = pending.find(id); it != pending.end())
            {
                const auto &error = field(j, "error");
                result(
                    it->second.ticket, ok,
                    ok ? "Accepted by Home Assistant"
                       : text(error, "message",
                              "Home Assistant refused it (" + text(error, "code", "error") + ")"),
                    ok ? field(j, "result") : Json());
                pending.erase(it);
                publish();
                return true;
            }
            if (id == catalog_id)
            {
                catalog_id = 0;
                if (!ok || !field(j, "result").is_array())
                {
                    state.status = "Cannot read Home Assistant's states. Check the user's access.";
                    publish();
                    return false;
                }
                state.catalog = catalog(field(j, "result"));
                if (!subscription)
                    return subscribe();
                publish();
            }
            else if (id == subscription)
            {
                if (!ok)
                {
                    state.status = "Cannot subscribe to states. Check entity access.";
                    publish();
                    return false;
                }
                subscribed = true;
                state.ready = state.ready || have_initial;
                state.status = state.ready ? "Connected" : "Reading Home Assistant";
                if (state.ready)
                    deadline = 0;
                attempts = 0;
                publish();
            }
            return true;
        };
        bool open =
            socket.open(config_.host, static_cast<std::uint16_t>(config_.port), "/api/websocket");
        if (open)
        {
            // The Date is whole seconds: the server's clock is half a second past it, on average.
            const auto server = parse_http_date(socket.server_date);
            if (server != kNoTime)
            {
                state.clock_skew_ms = server + 500 - wall_clock().utc_ms;
                state.clock_known = true;
            }
        }
        while (open && !stopping_)
        {
            if (!socket.tick())
                break;
            const auto now = monotonic_us();
            if ((deadline && now > deadline) || (ping_deadline && now > ping_deadline))
            {
                state.status = "Home Assistant timed out; reconnecting";
                break;
            }
            if (authorized && now - last_ping > 15000000)
            {
                ping_id = send({{"type", "ping"}});
                last_ping = now;
                ping_deadline = now + 10000000;
                if (!ping_id)
                    break;
            }
            bool resubscribe = false, recatalog = false;
            {
                std::lock_guard lock(mutex_);
                resubscribe = watch_changed_ && subscription;
                recatalog = catalog_due_ && authorized && !catalog_id;
                if (recatalog)
                    catalog_due_ = false;
            }
            if (resubscribe && !subscribe())
                break;
            if (recatalog && !(catalog_id = send({{"type", "get_states"}})))
                break;
            if (state.ready && pending.size() < 8)
            {
                Queued q;
                bool have = false;
                {
                    std::lock_guard lock(mutex_);
                    if (!queue_.empty())
                    {
                        q = std::move(queue_.front());
                        queue_.pop_front();
                        have = true;
                    }
                }
                if (have)
                {
                    if (q.generation != state.generation)
                    {
                        result(q.ticket, false, "Connection changed; not sent");
                        publish();
                    }
                    else if (now > q.expires)
                    {
                        result(q.ticket, false, "Not sent in time; try again");
                        publish();
                    }
                    else
                    {
                        const int message_id = send(q.request.message);
                        if (!message_id)
                        {
                            result(q.ticket, false, "Connection lost; the outcome is unknown");
                            break;
                        }
                        pending[message_id] = {q.ticket, now + q.request.timeout_us};
                    }
                }
            }
            bool expired = false;
            for (auto it = pending.begin(); it != pending.end();)
                if (now > it->second.expires)
                {
                    result(it->second.ticket, false, "No answer from Home Assistant yet");
                    it = pending.erase(it);
                    expired = true;
                }
                else
                    ++it;
            if (expired)
                publish();
            hui::sys::sleep_us(10000);
        }
        socket.close();
        for (const auto &[message_id, p] : pending)
            result(p.ticket, false, "Connection lost; the outcome is unknown");
        {
            // Closed to new requests and drained in one step, so nothing accepted now can be
            // sent on the next connection after the app was told it failed.
            std::lock_guard lock(mutex_);
            snapshot_.ready = false;
            snapshot_.connected = false;
            for (const auto &q : queue_)
                result(q.ticket, false, "Connection lost; not sent");
            queue_.clear();
        }
        state.ready = false;
        state.connected = false;
        state.states.clear();
        if (invalid_auth)
        {
            publish();
            return;
        }
        if (state.status == "Connected" || state.status == "Connecting to Home Assistant" ||
            state.status == "Reading Home Assistant")
            state.status = "Connection lost. Retrying automatically.";
        publish();
        const int seconds = std::min(30, 1 << std::min(attempts++, 5));
        for (int i = 0; i < seconds * 20 && !stopping_; ++i)
            hui::sys::sleep_us(50000);
    }
}
} // namespace lamplight
