// Lamplight for PS5 - A timer or an appliance that finishes takes over the screen until answered.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"

namespace lamplight
{
using hui::Action;
void Application::check_alerts()
{
    if (!ready())
        return;
    // View Assist's timers: one that runs out alerts once. View Assist may mark it expired, or drop
    // it from the list as it finishes; one that goes before its end was cancelled. Only timers seen
    // running in this session count, so an old expired one doesn't alert every time Lamplight
    // opens.
    const auto &list = field(snapshot_.states.get(config_.view_assist).attributes, "timers");
    auto finished = [&](const std::string &id, const Seen &seen)
    {
        const auto key = "va:" + id;
        if (alerted_.contains(key))
            return;
        alerted_.insert(key);
        raise_alert(Alert{key,
                          capitalised(seen.name.empty() ? seen.kind == "alarm"      ? "Alarm"
                                                          : seen.kind == "reminder" ? "Reminder"
                                                                                    : "Timer"
                                                        : seen.name),
                          seen.kind == "timer" ? "Time's up" : "It's " + hhmm(seen.at, clock_.zone),
                          seen.kind == "alarm"      ? "alarm"
                          : seen.kind == "reminder" ? "bell-ring-outline"
                                                    : "timer-outline",
                          id, now()});
    };
    std::set<std::string> listed;
    if (list.is_array())
        for (const auto &t : list)
        {
            const auto &id_json = field(t, "id");
            const auto id = id_json.is_string() ? id_json.get<std::string>() : id_json.dump();
            const auto at = parse_iso_ms(text(t, "expires"), clock_.zone);
            const auto status = text(t, "status");
            listed.insert(id);
            const bool done =
                status == "expired" || (status != "paused" && at != kNoTime && at <= now());
            auto it = seen_running_.find(id);
            if (!done)
            {
                // A paused timer's expiry is stale (it can't finish while paused): no end to
                // compare.
                seen_running_[id] = {at == kNoTime || status == "paused" ? 0 : at, text(t, "name"),
                                     text(t, "timer_class", "timer")};
                continue;
            }
            if (it != seen_running_.end())
            {
                finished(id, it->second);
                seen_running_.erase(it);
            }
        }
    for (auto it = seen_running_.begin(); it != seen_running_.end();)
    {
        if (listed.contains(it->first))
        {
            ++it;
            continue;
        }
        // Gone from the list: finished if its end has come (or nearly), cancelled if not.
        if (it->second.at && it->second.at <= now() + 5000)
            finished(it->first, it->second);
        it = seen_running_.erase(it);
    }
    // An appliance's timer that was running, and has reached its end or stopped.
    for (const auto &c : config_.appliances)
    {
        const auto &s = snapshot_.states.get(c.entity);
        const auto at = parse_iso_ms(text(s.attributes, "finishes_at"), clock_.zone);
        if (s.state == "active" && at != kNoTime)
        {
            if (at > now())
            {
                appliance_finish_[c.entity] = at;
                continue;
            }
        }
        auto it = appliance_finish_.find(c.entity);
        if (it == appliance_finish_.end())
            continue;
        const bool finished =
            it->second <= now() + 60000; // stopped early is a cancel, not a finish
        const auto key = "app:" + c.entity + ":" + std::to_string(it->second);
        appliance_finish_.erase(it);
        if (!finished || alerted_.contains(key))
            continue;
        alerted_.insert(key);
        raise_alert(Alert{key, c.name.empty() ? s.name(c.entity) : c.name, "Finished",
                          c.icon.empty() ? "timer-sand" : c.icon, "", now()});
    }
    if (alerted_.size() > 128)
        alerted_.clear();

    // Nobody answered for five minutes: the alert steps aside (the timer chip still says Done).
    if (alert_ && now() - alert_->since > 300000)
        next_alert();
}
bool Application::alert_still_listed() const
{
    if (!alert_ || alert_->timer_id.empty())
        return false;
    const auto &list = field(snapshot_.states.get(config_.view_assist).attributes, "timers");
    if (list.is_array())
        for (const auto &t : list)
        {
            const auto &id = field(t, "id");
            if ((id.is_string() ? id.get<std::string>() : id.dump()) == alert_->timer_id)
                return true;
        }
    return false;
}
void Application::raise_alert(Alert alert)
{
    if (alert_)
        waiting_alerts_.push_back(std::move(alert));
    else
        alert_ = std::move(alert);
}
void Application::next_alert()
{
    alert_.reset();
    if (waiting_alerts_.empty())
        return;
    alert_ = std::move(waiting_alerts_.front());
    alert_->since = now();
    waiting_alerts_.pop_front();
}
void Application::update_alert(const hui::InputFrame &input)
{
    if (input.is_pressed(Action::confirm))
    {
        // Stop: View Assist rings until its timer is cancelled. If the cancel can't be sent, the
        // alert stays, so it can be tried again (2: the ringing would go on unseen).
        if (alert_still_listed() &&
            !dispatch(service("view_assist", "cancel_timer", {{"timer_id", alert_->timer_id}}),
                      Job::cancel_timer))
        {
            toast("Not connected; it's still ringing. Try again.", true, 5000);
            return;
        }
        next_alert();
    }
    else if (input.is_pressed(Action::back))
        next_alert();
}
} // namespace lamplight
