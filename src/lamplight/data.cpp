// Lamplight for PS5 - The forecast and the calendars: when to fetch, and what came back.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/assistant.hpp"
#include "lamplight/resources.hpp"

namespace lamplight
{
namespace
{
constexpr std::int64_t kForecastMs = 20 * 60000, kForecastRetryMs = 60000;
constexpr std::int64_t kAgendaMs = 10 * 60000;
constexpr std::size_t kMaxPhotos = 2000, kMaxPhotoFolders = 64;
// DESIGN.md, Load states: something to show beats an error.
std::string load_state(bool loading, bool error, std::size_t count)
{
    return count > 0 ? "ready" : error ? "failed" : loading ? "loading" : "empty";
}
} // namespace

std::string Application::forecast_state() const
{
    const auto hours = next_hours(forecast_.hours, now());
    return load_state(forecast_loading_ || (!forecast_.at && !forecast_failed_at_),
                      !forecast_loading_ && forecast_failed_at_,
                      hours.size() >= 2 ? hours.size() : 0);
}
std::string Application::agenda_state() const
{
    if (!agenda_loaded_)
        return "loading";
    return load_state(false, agenda_failed_ > 0, static_cast<std::size_t>(agenda_read_));
}
void Application::load_forecast(bool force)
{
    if (forecast_loading_ || config_.weather.empty())
        return;
    if (!force && forecast_.at && now() - forecast_.at < kForecastMs)
        return;
    if (!force && forecast_failed_at_ && now() - forecast_failed_at_ < kForecastRetryMs)
        return;
    forecast_loading_ = true;
    forecast_parts_ = 2;
    forecast_failed_part_ = false;
    for (const char *kind : {"hourly", "daily"})
        if (!dispatch(service("weather", "get_forecasts", {{"type", kind}}, config_.weather, true),
                      std::string(kind) == "hourly" ? Job::hours : Job::days))
        {
            forecast_failed_part_ = true;
            --forecast_parts_;
        }
    if (!forecast_parts_)
        finish_forecast();
}
void Application::finish_forecast()
{
    forecast_loading_ = false;
    if (forecast_failed_part_)
        // A refresh that fails keeps the last forecast (The Last Good Data Rule).
        forecast_failed_at_ = now();
    else
    {
        forecast_.hours = std::move(hours_loading_);
        forecast_.days = std::move(days_loading_);
        forecast_.at = now();
        forecast_failed_at_ = 0;
    }
    hours_loading_.clear();
    days_loading_.clear();
}
void Application::load_agenda()
{
    if (agenda_loading_ || config_.calendars.empty())
        return;
    std::string signature;
    for (const auto &c : config_.calendars)
    {
        const auto &e = snapshot_.states.get(c.entity);
        signature += c.entity + "|" + e.state + "|" + text(e.attributes, "message") + ",";
    }
    const auto today = day_start(now(), clock_.zone);
    if (agenda_loaded_ && signature == calendar_signature_ && now() - agenda_at_ < kAgendaMs &&
        today == agenda_day_)
        return;
    calendar_signature_ = signature;
    agenda_loading_ = true;
    agenda_waiting_ = 0;
    agenda_failed_loading_ = 0;
    events_loading_.clear();
    agenda_day_ = today;
    const auto from = iso_local(today, clock_.zone);
    const auto to = iso_local(next_day(next_day(today, clock_.zone), clock_.zone), clock_.zone);
    // One call per calendar, so a broken calendar can't blank the page.
    for (const auto &c : config_.calendars)
        if (dispatch(service("calendar", "get_events",
                             {{"start_date_time", from}, {"end_date_time", to}}, c.entity, true),
                     Job::events, c.entity))
            ++agenda_waiting_;
        else
            ++agenda_failed_loading_;
    if (!agenda_waiting_)
        finish_agenda();
}
void Application::finish_agenda()
{
    agenda_loading_ = false;
    agenda_loaded_ = true;
    agenda_at_ = now();
    const int read = static_cast<int>(config_.calendars.size()) - agenda_failed_loading_;
    // A refresh where every calendar failed keeps what was on screen (The Last Good Data Rule).
    if (read > 0 || agenda_read_ == 0)
    {
        events_ = std::move(events_loading_);
        agenda_read_ = read;
        agenda_failed_ = agenda_failed_loading_;
    }
    events_loading_.clear();
    agenda_revealed_ = false;
}
void Application::fail_pending(const std::string &why)
{
    for (const auto &[ticket, job] : std::map<std::uint64_t, Pending>(pending_))
        handle({ticket, false, why, {}});
}
void Application::handle(const Result &r)
{
    auto it = pending_.find(r.ticket);
    if (it == pending_.end())
        return;
    const auto job = it->second;
    pending_.erase(it);
    const auto &response = field(r.response, "response");
    switch (job.job)
    {
    case Job::hours:
    case Job::days:
    {
        const bool ok =
            r.ok && (job.job == Job::hours
                         ? parse_hours(response, config_.weather, clock_.zone, hours_loading_)
                         : parse_days(response, config_.weather, clock_.zone, days_loading_));
        if (!ok)
            forecast_failed_part_ = true;
        if (--forecast_parts_ <= 0)
            finish_forecast();
        break;
    }
    case Job::events:
        if (!r.ok || !parse_events(response, job.key, clock_.zone, events_loading_))
            ++agenda_failed_loading_;
        if (--agenda_waiting_ <= 0)
            finish_agenda();
        break;
    case Job::start_timer:
    case Job::cancel_timer:
    case Job::undo_timer:
    case Job::keep:
    case Job::room:
    case Job::setting:
        if (job.job == Job::room)
            room_busy_.clear();
        if (!r.ok)
        {
            busy_minutes_ = 0;
            toast("That didn't work: " + r.message, true, 5000);
        }
        break;
    case Job::said:
    {
        // A script may answer in its own words, and may refuse in them.
        if (!r.ok)
            speaker_move_.reset();
        const auto words = text(response, "message");
        if (!r.ok)
            toast("That didn't work: " + r.message, true, 5000);
        else if (field(response, "ok") == false)
            toast(words.empty() ? std::string("That didn't work") : words, true, 5000);
        // A sentence ("Warming the living room to 21° for 2 hours") stays long enough to read
        // from the sofa.
        else if (!words.empty())
            toast(words, false, words.size() > 24 ? 4500 : 2500);
        else if (!job.key.empty())
            toast(job.key);
        break;
    }
    case Job::history:
        if (job.key != chart_.entity)
            break;
        chart_.loading = false;
        chart_.failed = !r.ok;
        if (r.ok)
        {
            chart_.points = history_points(r.response);
            chart_.at = now();
        }
        break;
    case Job::undoable:
        if (r.ok)
            offer_lamps_undo(job.key);
        else
            toast("That didn't work: " + r.message, true, 5000);
        break;
    case Job::snapshot:
        if (waiting_action_)
        {
            const auto action = std::move(*waiting_action_);
            waiting_action_.reset();
            finish_house_action(action, r.ok);
        }
        break;
    case Job::dismiss:
        if (!r.ok)
            toast("Couldn't dismiss it: " + r.message +
                      (r.message.find("Unauthorized") != std::string::npos
                           ? ". Dismissing needs an administrator's token."
                           : ""),
                  true, 6000);
        break;
    case Job::photo_list:
        photo_busy_ = false;
        if (r.ok)
        {
            for (auto &id : photo_ids(r.response))
                if (photo_found_.size() < kMaxPhotos)
                    photo_found_.push_back(std::move(id));
            for (auto &[id, title] : photo_folders(r.response))
                if (photo_walked_.size() < kMaxPhotoFolders && photo_walked_.insert(id).second)
                    photo_walk_.push_back(std::move(id));
        }
        else
            photo_walk_failed_ = true;
        if (photo_walk_.empty())
            finish_photo_walk();
        break;
    case Job::photo_url:
        photo_busy_ = false;
        if (r.ok && !text(r.response, "url").empty())
            want_picture(Pic::photo, ha_url(text(r.response, "url")) + kSoftPicture, true);
        else
            photo_failed();
        break;
    case Job::folder:
        folder_listed(r, job.key);
        break;
    case Job::ask:
        if (r.ticket == asking_)
        {
            asking_ = 0;
            asking_example_ = -1;
        }
        if (!r.ok)
        {
            toast("That didn't work: " + r.message, true, 5000);
            break;
        }
        {
            auto reply = text(field(field(response, "speech"), "plain"), "speech");
            if (reply.empty())
                reply = "Done";
            const auto conversation = text(r.response, "conversation_id");
            if (!conversation.empty() && job.key != "other-agent")
            {
                conversation_ = conversation;
                conversation_at_ = now();
            }
            answer_ = reply;
            answer_title_.clear();
            answer_image_.clear();
            answer_at_ = now();
            info_scroll_ = 0;
            toast(plain_reply(reply), false, 7000, std::nullopt, "", true);
        }
        break;
    }
}
} // namespace lamplight
