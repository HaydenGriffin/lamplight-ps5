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
};
// One websocket message without its id, for example
// {"type":"call_service","domain":"weather","service":"get_forecasts",...}.
struct Request
{
    Json message;
    std::int64_t timeout_us = 15000000;
};
// Requests are allowed only for the services and commands Lamplight uses, and for the room page's
// own entities the services a press calls there (room.hpp).
bool allowed(const Json &message, const Config &config);
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
    // Subscribe to these entities instead (Setup changed what the pages show).
    void watch(std::set<std::string> watched);
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
    void run();
    mutable std::mutex mutex_;
    Snapshot snapshot_;
    Config config_;
    std::set<std::string> watched_;
    bool watch_changed_ = false, catalog_due_ = false;
    std::deque<Queued> queue_;
    std::atomic<bool> stopping_{true};
    pthread_t thread_{};
    bool started_ = false;
    std::uint64_t ticket_ = 0;
};
} // namespace lamplight
