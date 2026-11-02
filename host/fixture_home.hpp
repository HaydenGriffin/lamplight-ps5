// Lamplight for PS5 - A pretend Home Assistant from tests/fixture.json, for the preview and tests.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/application.hpp"
#include <fstream>

namespace lamplight
{
// Records what the app sends and answers it from the fixture, as Home Assistant would.
struct FixtureHome
{
    Json fixture;
    Snapshot snapshot;
    std::vector<std::pair<std::uint64_t, Json>> sent;
    std::uint64_t ticket = 0;
    bool fail_forecast = false, fail_calendars = false, answer = true;
    WallClock clock;

    explicit FixtureHome(const char *path = "tests/fixture.json")
    {
        std::ifstream file(path);
        fixture = Json::parse(file, nullptr, false);
        clock.utc_ms = parse_iso_ms(text(fixture, "now"), Zone{});
        snapshot.ready = snapshot.connected = true;
        snapshot.generation = 1;
        snapshot.status = "Connected";
        snapshot.states.apply(fixture["states"]);
        snapshot.catalog = catalog(fixture["catalog"]);
    }
    Config config() const
    {
        return Config::parse(fixture["config"]);
    }
    void wire(Application &app)
    {
        app.send = [this](Request r)
        {
            if (!snapshot.ready)
                return std::uint64_t{0};
            sent.push_back({++ticket, r.message});
            return ticket;
        };
    }
    // Answers everything sent since the last call, then hands the app a new snapshot.
    void reply(Application &app)
    {
        for (auto &[t, m] : sent)
        {
            Result r{t, true, "Accepted by Home Assistant", Json::object()};
            const auto call = text(m, "domain") + "." + text(m, "service");
            const auto entity = text(field(m, "target"), "entity_id");
            if (call == "weather.get_forecasts")
            {
                r.ok = !fail_forecast;
                r.response = {
                    {"response", fixture["forecasts"][text(field(m, "service_data"), "type")]}};
            }
            else if (call == "calendar.get_events")
            {
                r.ok = !fail_calendars;
                r.response = {{"response", fixture["events"][entity]}};
            }
            else if (text(m, "type") == "conversation/process")
                r.response = fixture["reply"];
            if (!r.ok)
                r.message = "Service unavailable";
            if (answer)
                snapshot.results.push_back(std::move(r));
        }
        sent.clear();
        ++snapshot.revision;
        app.receive(snapshot);
    }
    // A frame with no buttons, or with one pressed.
    void frame(Application &app, hui::InputFrame input = {}, float dt = 1.0f / 60)
    {
        input.connected = true;
        hui::ui::Feedback feedback;
        app.update(input, dt, clock, feedback);
    }
    void press(Application &app, hui::Action action)
    {
        hui::InputFrame in;
        in.pressed = in.held = hui::action_bit(action);
        frame(app, in);
        frame(app);
    }
    void move(Application &app, hui::Direction direction)
    {
        hui::InputFrame in;
        in.nav = direction;
        frame(app, in);
    }
    // Lets the app send its first requests and get the answers.
    void settle(Application &app)
    {
        for (int i = 0; i < 3; ++i)
        {
            reply(app);
            frame(app);
        }
        reply(app);
    }
};
} // namespace lamplight
