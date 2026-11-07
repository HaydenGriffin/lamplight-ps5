// Lamplight for PS5 - Reconnecting Home Assistant session: states, catalogue and requests.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/config.hpp"
#include "lamplight/states.hpp"
#include <atomic>
#include <deque>
#include <mutex>
#include <pthread.h>
#include <set>

namespace lamplight
{
struct Result
{
    std::uint64_t ticket = 0;
    bool ok = false;
    std::string message;
    Json response; // the result's "result": a service response or the assistant's answer
};
// Push-to-talk: the microphone through Home Assistant's Assist pipeline, from speech to an answer.
struct Assist
{
    enum class Stage
    {
        idle,
        starting,  // asked Home Assistant for a run
        listening, // it takes the microphone now
        thinking,  // it heard the words; the answer is coming
        done,
        failed
    };
    std::uint64_t run = 0; // the listen() this is about
    Stage stage = Stage::idle;
    std::string heard, reply, conversation, error;
};
struct Snapshot
{
    bool connected = false, ready = false;
    std::uint64_t generation = 0, revision = 0;
    std::string status = "Enter your Home Assistant address and token";
    StateTable states;
    std::vector<CatalogEntry> catalog;
    std::vector<Result> results;
    // Home Assistant's clock less the console's, from the server's Date when the socket opened
    // (a console's clock can be minutes out, and countdowns are worked out against it).
    std::int64_t clock_skew_ms = 0;
    bool clock_known = false;
    std::string country; // Home Assistant's, for the clock's 12 or 24 hours
    Assist assist;
};
// One websocket message without its id, for example
// {"type":"call_service","domain":"weather","service":"get_forecasts",...}.
struct Request
{
    Json message;
    std::int64_t timeout_us = 15000000;
    // A history request (history.hpp): its answer is made small on the network thread.
    std::string history_entity = {}, history_attribute = {};
};
// The scene an action's Undo puts the lamps back from.
inline constexpr const char *kUndoSceneId = "lamplight_ps5_undo";
// One a setting names, made as it says.
Request call_request(const ServiceCall &call);
// Requests are allowed only for the services and commands Lamplight uses, for the room page's own
// entities the services a press or a sheet calls there (controls.hpp), and for the calls settings
// name, exactly as they name them. watched adds the lamps inside a room's light groups, which only
// Home Assistant's states name; a chart can read only what is watched.
bool allowed(const Json &message, const Config &config, const std::set<std::string> &watched = {});
Request service(const std::string &domain, const std::string &service, Json data,
                const std::string &entity = "", bool response = false);
Request ask(const std::string &text, const std::string &agent, const std::string &conversation);

class Client
{
  public:
    ~Client();
    bool start(const Config &config, std::set<std::string> watched);
    void stop();
    Snapshot snapshot() const;
    // Zero means refused (not connected, queue full, not allowed). Accepted requests are never
    // replayed after a reconnect: they fail with "Connection lost".
    std::uint64_t send(Request request);
    // Subscribe to these entities instead, and allow requests for what these settings show
    // (Setup or Home Assistant's settings entity changed them).
    void watch(const Config &config, std::set<std::string> watched);
    // Push-to-talk: start a run (0 when not connected), give it the microphone's samples (16 kHz
    // mono; any thread), then end it. What was heard and answered arrives in snapshot().assist.
    // Samples given with no run going are kept, the last half second, and start the next run: the
    // app starts one only once someone is speaking, without losing the first syllable.
    std::uint64_t listen(const std::string &pipeline, const std::string &conversation);
    void audio(const std::int16_t *samples, std::size_t count);
    void end_audio();
    // Read the catalogue again (Setup opened a list of entities).
    void refresh_catalog();

  private:
    struct Queued
    {
        Request request;
        std::uint64_t ticket = 0;
        std::int64_t expires = 0;
        std::uint64_t generation = 0; // the connection it was accepted for
    };
    struct Listen
    {
        std::uint64_t run = 0;
        std::string pipeline, conversation;
        bool start = false, ended = false;
        std::deque<std::int16_t> samples;
    };
    void run();
    mutable std::mutex mutex_;
    Snapshot snapshot_;
    Config config_; // the connection (the worker reads it)
    Config allow_;  // what requests may touch (under mutex_)
    Listen listen_;
    std::deque<std::int16_t> preroll_;
    std::set<std::string> watched_;
    bool watch_changed_ = false, catalog_due_ = false;
    std::deque<Queued> queue_;
    std::atomic<bool> stopping_{true};
    pthread_t thread_{};
    bool started_ = false;
    std::uint64_t ticket_ = 0;
};
} // namespace lamplight
