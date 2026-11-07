// Lamplight for PS5 - Reconnecting Home Assistant session: states, catalogue and requests.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/client.hpp"
#include "lamplight/controls.hpp"
#include "lamplight/clock.hpp"
#include "lamplight/history.hpp"
#include "lamplight/speakers.hpp"
#include "afterglow/ma_socket.hpp"
#include "platform/ps5/system.hpp"
#include <algorithm>
#include <map>
#include <string_view>

namespace lamplight
{
using hui::sys::monotonic_us;
namespace
{
// Inside the folder, not a sibling whose name starts the same way (photos and photos_old). Media
// ids go on with "/", or "|" and ":" for integrations such as Immich.
bool inside_folder(const std::string &id, const std::string &folder)
{
    if (folder.empty() || !id.starts_with(folder))
        return false;
    if (id.size() == folder.size())
        return true;
    const auto separator = std::string_view("/|:");
    return separator.find(folder.back()) != std::string_view::npos ||
           separator.find(id[folder.size()]) != std::string_view::npos;
}
bool same_call(const ServiceCall &call, const std::string &name, const Json &data,
               const std::string &entity)
{
    return !call.empty() && call.service == name && call.entity == entity && call.data == data;
}
// One of the calls the settings name, exactly as they name it.
bool named_in_settings(const Config &config, const std::string &name, const Json &data,
                       const std::string &entity)
{
    auto same = [&](const ServiceCall &call) { return same_call(call, name, data, entity); };
    auto same_named = [&](const std::vector<NamedCall> &list) {
        return std::any_of(list.begin(), list.end(),
                           [&](const NamedCall &c) { return same(c.call); });
    };
    if (same_named(config.house.actions) ||
        std::any_of(config.house.modes.begin(), config.house.modes.end(),
                    [&](const HouseMode &m) { return same(m.on) || same(m.off); }))
        return true;
    return std::any_of(config.rooms.begin(), config.rooms.end(),
                       [&](const RoomPage &room)
                       {
                           return std::any_of(room.scenes.begin(), room.scenes.end(),
                                              [&](const RoomScene &s) { return same(s.call); }) ||
                                  std::any_of(room.tiles.begin(), room.tiles.end(),
                                              [&](const RoomThing &t)
                                              { return same_named(t.boosts); });
                       });
}
bool is_speaker(const Config &config, const std::string &player)
{
    if (!player.empty() && player == config.speakers.group)
        return true;
    const auto list = speaker_list(config);
    // A room's own speaker counts even where settings list others: Now Playing can be showing it.
    return std::any_of(list.begin(), list.end(),
                       [&](const Speaker &s) { return s.player == player; }) ||
           std::any_of(config.rooms.begin(), config.rooms.end(),
                       [&](const RoomPage &room) { return room.media == player; });
}
// What the speakers sheet alone may ask: moving the music between them, and their sleep timer.
bool speaker_call(const Config &config, const std::string &call, const Json &data,
                  const std::string &entity)
{
    if (call == "music_assistant.transfer_queue")
        return is_speaker(config, entity) && is_speaker(config, text(data, "source_player"));
    if (!config.speakers.sleep_script.empty() && call == config.speakers.sleep_script)
    {
        const auto &minutes = field(data, "minutes");
        return is_speaker(config, text(data, "player")) && minutes.is_number_integer() &&
               minutes >= 0 && minutes <= 720;
    }
    if (call == "media_player.join")
    {
        const auto joining = strings(field(data, "group_members"));
        return !entity.empty() && entity == config.speakers.group && !joining.empty() &&
               std::all_of(joining.begin(), joining.end(),
                           [&](const std::string &p) { return is_speaker(config, p); });
    }
    return false;
}
// The lamps an action is about to change, noted so Undo can put them back.
bool undo_snapshot(const Json &data, const std::set<std::string> &watched)
{
    const auto lamps = strings(field(data, "snapshot_entities"));
    return text(data, "scene_id") == kUndoSceneId && !lamps.empty() &&
           std::all_of(lamps.begin(), lamps.end(), [&](const std::string &lamp)
                       { return domain_of(lamp) == "light" && watched.contains(lamp); });
}
// Something the settings put on a page, by entity.
bool on_a_page(const Config &config, const std::string &entity)
{
    if (is_speaker(config, entity))
        return true;
    for (const auto &page : config.rooms)
    {
        if (page.media == entity || page.light == entity || room_light(page) == entity)
            return true;
        for (const auto *list : {&page.tiles, &page.actions})
            for (const auto &t : *list)
                if (t.entity == entity)
                    return true;
    }
    return std::any_of(config.house.modes.begin(), config.house.modes.end(),
                       [&](const HouseMode &m) { return m.entity == entity; });
}
} // namespace
bool allowed(const Json &m, const Config &config, const std::set<std::string> &watched)
{
    const auto type = text(m, "type");
    if (type == "conversation/process")
        return !text(m, "text").empty();
    // Dismissing a notice: only the event the settings name.
    if (type == "fire_event")
        return !config.notices.dismiss_event.empty() &&
               text(m, "event_type") == config.notices.dismiss_event;
    // Browsing only lists names (Setup's folder chooser starts at the top); a picture's address
    // comes only from inside the folder Setup names.
    if (type == "media_source/browse_media")
    {
        const auto id = text(m, "media_content_id");
        return id.empty() || id.starts_with("media-source://");
    }
    if (type == "media_source/resolve_media")
        return inside_folder(text(m, "media_content_id"), config.photos);
    // A chart reads only what a page already shows.
    if (type == "history/history_during_period")
    {
        const auto ids = strings(field(m, "entity_ids"));
        return ids.size() == 1 && watched.contains(ids[0]);
    }
    if (type != "call_service")
        return false;
    const auto domain = text(m, "domain");
    const auto name = text(m, "service");
    const auto call = domain + "." + name;
    if (call == "weather.get_forecasts" || call == "calendar.get_events" ||
        call == "view_assist.set_timer" || call == "view_assist.cancel_timer" ||
        call == "view_assist.set_state")
        return true;
    const auto entity = text(field(m, "target"), "entity_id");
    const auto &data = field(m, "service_data");
    if (named_in_settings(config, call, data, entity) || speaker_call(config, call, data, entity))
        return true;
    if (call == "scene.create")
        return undo_snapshot(data, watched);
    if (call == "scene.turn_on" && entity == std::string("scene.") + kUndoSceneId)
        return true;
    if (domain_of(entity) != domain || !control_service(domain, name))
        return false;
    // media_player.join goes through speaker_call, which looks at who is joining.
    if (call == "media_player.join")
        return false;
    return on_a_page(config, entity) || (domain == "light" && watched.contains(entity));
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
Request call_request(const ServiceCall &call)
{
    const auto dot = call.service.find('.');
    return service(call.service.substr(0, dot), call.service.substr(dot + 1), call.data,
                   call.entity, call.response);
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
// One event of an Assist run: what it heard, what it answered, or why it stopped.
void assist_event(const Json &event, Assist &assist, int &handler)
{
    const auto type = text(event, "type");
    const auto &data = field(event, "data");
    if (type == "run-start")
    {
        const auto &id = field(field(data, "runner_data"), "stt_binary_handler_id");
        if (id.is_number_integer() && id >= 0 && id <= 255)
        {
            handler = id.get<int>();
            assist.stage = Assist::Stage::listening;
        }
        else
        {
            assist.stage = Assist::Stage::failed;
            assist.error = "Home Assistant's pipeline has no speech-to-text";
        }
    }
    else if (type == "stt-end")
    {
        // Whisper starts what it heard with a space.
        assist.heard = text(field(data, "stt_output"), "text");
        assist.heard.erase(0, assist.heard.find_first_not_of(" \t\n"));
        assist.heard.erase(assist.heard.find_last_not_of(" \t\n") + 1);
        assist.stage = Assist::Stage::thinking;
    }
    else if (type == "intent-end")
    {
        const auto &output = field(data, "intent_output");
        assist.reply = text(field(field(field(output, "response"), "speech"), "plain"), "speech");
        assist.conversation = text(output, "conversation_id");
    }
    else if (type == "error")
    {
        assist.stage = Assist::Stage::failed;
        assist.error = text(data, "message", "Home Assistant couldn't finish");
    }
    else if (type == "run-end" && assist.stage != Assist::Stage::failed)
        assist.stage = Assist::Stage::done;
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
        allow_ = config;
        const auto run = listen_.run;
        listen_ = {};
        listen_.run = run;
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
    if (!snapshot_.ready || queue_.size() >= 16 || !allowed(r.message, allow_, watched_))
        return 0;
    const auto ticket = ++ticket_;
    queue_.push_back({std::move(r), ticket, monotonic_us() + 5000000, snapshot_.generation});
    return ticket;
}
void Client::watch(const Config &config, std::set<std::string> watched)
{
    std::lock_guard lock(mutex_);
    allow_ = config;
    if (watched == watched_)
        return;
    watched_ = std::move(watched);
    watch_changed_ = true;
}
std::uint64_t Client::listen(const std::string &pipeline, const std::string &conversation)
{
    std::lock_guard lock(mutex_);
    if (!snapshot_.ready)
        return 0;
    listen_ = {listen_.run + 1, pipeline, conversation, true, false, std::move(preroll_)};
    preroll_.clear();
    return listen_.run;
}
void Client::audio(const std::int16_t *samples, std::size_t count)
{
    std::lock_guard lock(mutex_);
    if (!listen_.run || listen_.ended)
    {
        preroll_.insert(preroll_.end(), samples, samples + count);
        while (preroll_.size() > 8000)
            preroll_.pop_front();
        return;
    }
    // Fifteen seconds at most: a held button longer than that is not a question.
    if (listen_.samples.size() + count > 16000 * 15)
        return;
    listen_.samples.insert(listen_.samples.end(), samples, samples + count);
}
void Client::end_audio()
{
    std::lock_guard lock(mutex_);
    listen_.ended = true;
    preroll_.clear();
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
            state.country = snapshot_.country;
            state.assist = snapshot_.assist;
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
        int next = 1, catalog_id = 0, subscription = 0, ping_id = 0, config_id = 0;
        // Push-to-talk: the run's message id, and the byte that marks its audio (-1 until known).
        int listen_id = 0, handler = -1;
        // A run replaced before Home Assistant said it started: ended as soon as it does.
        int orphan_id = 0;
        bool audio_ended = false;
        bool authorized = false, invalid_auth = false, subscribed = false, have_initial = false;
        std::set<std::string> subscribed_ids;
        struct Pending
        {
            std::uint64_t ticket;
            std::int64_t expires;
            std::string history_entity = {}, history_attribute = {};
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
                config_id = send({{"type", "get_config"}});
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
                if (orphan_id && id == orphan_id)
                {
                    const auto &event = field(j, "event");
                    const auto &number =
                        field(field(field(event, "data"), "runner_data"), "stt_binary_handler_id");
                    if (text(event, "type") == "run-start" && number.is_number_integer() &&
                        number >= 0 && number <= 255)
                    {
                        orphan_id = 0;
                        const auto last = static_cast<std::uint8_t>(number.get<int>());
                        return socket.send_binary(&last, 1);
                    }
                    if (text(event, "type") == "run-end" || text(event, "type") == "error")
                        orphan_id = 0;
                    return true;
                }
                if (listen_id && id == listen_id)
                {
                    assist_event(field(j, "event"), state.assist, handler);
                    if (state.assist.stage == Assist::Stage::done ||
                        state.assist.stage == Assist::Stage::failed)
                        listen_id = 0;
                    publish();
                    return true;
                }
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
            if (config_id && id == config_id)
            {
                config_id = 0;
                state.country = text(field(j, "result"), "country");
                publish();
                return true;
            }
            if (listen_id && id == listen_id)
            {
                if (!ok)
                {
                    state.assist.stage = Assist::Stage::failed;
                    state.assist.error = text(field(j, "error"), "message",
                                              "Home Assistant couldn't start listening");
                    listen_id = 0;
                    publish();
                }
                return true;
            }
            if (auto it = pending.find(id); it != pending.end())
            {
                const auto &error = field(j, "error");
                result(
                    it->second.ticket, ok,
                    ok ? "Accepted by Home Assistant"
                       : text(error, "message",
                              "Home Assistant refused it (" + text(error, "code", "error") + ")"),
                    !ok ? Json()
                    : it->second.history_entity.empty()
                        ? field(j, "result")
                        : compact_history(field(j, "result"), it->second.history_entity,
                                          it->second.history_attribute));
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
        // Push-to-talk: start a run that was asked for, then send what the microphone gave it.
        auto pump_listen = [&]
        {
            std::string pipeline, conversation;
            std::uint64_t run = 0;
            bool start = false, end = false;
            std::vector<std::uint8_t> frame;
            {
                std::lock_guard lock(mutex_);
                if (listen_.start)
                {
                    start = true;
                    listen_.start = false;
                    run = listen_.run;
                    pipeline = listen_.pipeline;
                    conversation = listen_.conversation;
                }
                else if (listen_id && handler >= 0 && !audio_ended &&
                         listen_.run == state.assist.run)
                {
                    const auto n = std::min<std::size_t>(listen_.samples.size(), 2048);
                    if (n)
                    {
                        frame.resize(1 + n * 2);
                        frame[0] = static_cast<std::uint8_t>(handler);
                        for (std::size_t i = 0; i < n; ++i)
                        {
                            const auto v = static_cast<std::uint16_t>(listen_.samples[i]);
                            frame[1 + i * 2] = static_cast<std::uint8_t>(v & 0xff);
                            frame[2 + i * 2] = static_cast<std::uint8_t>(v >> 8);
                        }
                        listen_.samples.erase(listen_.samples.begin(),
                                              listen_.samples.begin() + static_cast<long>(n));
                    }
                    end = listen_.ended && listen_.samples.empty();
                }
            }
            if (start)
            {
                // A run still taking audio is ended first, so Home Assistant lets it go (or, if
                // it hasn't started yet, as soon as it does).
                if (listen_id && handler >= 0 && !audio_ended)
                {
                    const auto last = static_cast<std::uint8_t>(handler);
                    if (!socket.send_binary(&last, 1))
                        return false;
                }
                else if (listen_id && handler < 0)
                    orphan_id = listen_id;
                Json m{{"type", "assist_pipeline/run"},
                       {"start_stage", "stt"},
                       {"end_stage", "intent"},
                       {"input", {{"sample_rate", 16000}}}};
                if (!pipeline.empty())
                    m["pipeline"] = pipeline;
                if (!conversation.empty())
                    m["conversation_id"] = conversation;
                listen_id = send(std::move(m));
                handler = -1;
                audio_ended = false;
                state.assist = {run, Assist::Stage::starting, "", "", "", ""};
                publish();
                return listen_id != 0;
            }
            if (!frame.empty() && !socket.send_binary(frame.data(), frame.size()))
                return false;
            if (end)
            {
                // A frame of the handler alone ends the audio.
                const auto last = static_cast<std::uint8_t>(handler);
                audio_ended = true;
                return socket.send_binary(&last, 1);
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
            if (state.ready && !pump_listen())
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
                        pending[message_id] = {q.ticket, now + q.request.timeout_us,
                                               q.request.history_entity,
                                               q.request.history_attribute};
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
            // A run asked for but not started, and its audio, are never sent on the next one.
            if (listen_.start)
            {
                state.assist = {listen_.run, Assist::Stage::starting, "", "", "", ""};
                listen_.start = false;
            }
            listen_.samples.clear();
            listen_.ended = true;
        }
        state.ready = false;
        state.connected = false;
        state.states.clear();
        if (state.assist.stage == Assist::Stage::starting ||
            state.assist.stage == Assist::Stage::listening ||
            state.assist.stage == Assist::Stage::thinking)
        {
            state.assist.stage = Assist::Stage::failed;
            state.assist.error = "Lost the connection to Home Assistant";
        }
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
