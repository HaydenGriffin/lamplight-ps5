// Lamplight for PS5 - What the controller does on the Agenda, Timers, Help and Info pages.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/room.hpp"
#include <algorithm>

namespace lamplight
{
using hui::Action;
using hui::Direction;
namespace
{
constexpr std::int64_t kArmMs = 5000, kHelpArmMs = 4000, kFollowUpMs = 90000, kUndoMs = 8000;
int step(Direction nav, Direction down, Direction up)
{
    return nav == down ? 1 : nav == up ? -1 : 0;
}
} // namespace

void Application::update_agenda(const hui::InputFrame &input)
{
    if (agenda_state() == "failed")
    {
        if (input.is_pressed(Action::confirm))
        {
            agenda_loaded_ = false;
            agenda_read_ = 0;
            calendar_signature_.clear();
            load_agenda();
        }
        return;
    }
    if (input.nav == Direction::left || input.nav == Direction::right)
        agenda_column_ = input.nav == Direction::left ? 0 : 1;
    // Up and Down scroll the column with the focus, one event at a time; the view clamps it.
    const int move = step(input.nav, Direction::down, Direction::up);
    if (move)
    {
        if (!agenda_revealed_)
            for (int c = 0; c < 2; ++c)
                agenda_scroll_[c] = agenda_first(c);
        agenda_revealed_ = true;
        agenda_scroll_[agenda_column_] =
            std::clamp(agenda_scroll_[agenda_column_] + move, 0, agenda_last_first(agenda_column_));
    }
}
std::vector<Application::TimerSlot> Application::timer_slots() const
{
    std::vector<TimerSlot> slots;
    for (const auto &t :
         view_assist_timers(snapshot_.states.get(config_.view_assist), now(), clock_.zone))
        slots.push_back({0, t.id, 0});
    const auto apps = appliance_timers(config_.appliances, snapshot_.states, clock_.zone);
    for (std::size_t i = 0; i < apps.size(); ++i)
        slots.push_back({0, "", 0, true});
    for (int m : config_.quick_minutes)
        slots.push_back({1, "", m});
    return slots;
}
void Application::update_timers(const hui::InputFrame &input)
{
    auto slots = timer_slots();
    int counts[2] = {0, 0};
    for (const auto &s : slots)
        ++counts[s.column];
    if (!counts[timer_column_])
        timer_column_ = counts[0] ? 0 : 1;
    // The quick-start buttons are a grid of two columns.
    if (timer_column_ == 1)
    {
        int &at = timer_row_[1];
        if (input.nav == Direction::left && at % 2 == 0 && counts[0])
            timer_column_ = 0;
        else if (input.nav == Direction::left && at % 2 == 1)
            --at;
        else if (input.nav == Direction::right && at % 2 == 0 && at + 1 < counts[1])
            ++at;
        else if (input.nav == Direction::up && at >= 2)
            at -= 2;
        else if (input.nav == Direction::down && at + 2 < counts[1])
            at += 2;
    }
    else
    {
        if (input.nav == Direction::right && counts[1])
            timer_column_ = 1;
        else
            timer_row_[0] =
                std::clamp(timer_row_[0] + step(input.nav, Direction::down, Direction::up), 0,
                           std::max(0, counts[0] - 1));
    }
    timer_row_[0] = std::clamp(timer_row_[0], 0, std::max(0, counts[0] - 1));
    timer_row_[1] = std::clamp(timer_row_[1], 0, std::max(0, counts[1] - 1));
    if (!input.is_pressed(Action::confirm) || slots.empty())
        return;
    int index = timer_row_[timer_column_];
    for (const auto &s : slots)
        if (s.column == timer_column_ && index-- == 0)
        {
            if (s.appliance)
                return;
            if (s.timer_id.empty())
                start_timer(s.minutes);
            else
                cancel_timer(s.timer_id);
            return;
        }
}
void Application::start_timer(int minutes)
{
    if (config_.view_assist.empty())
    {
        toast("Choose a View Assist device in Setup to set timers here", true, 5000);
        return;
    }
    if (busy_minutes_)
        return;
    const auto words = std::to_string(minutes) + (minutes == 1 ? " minute" : " minutes");
    if (!dispatch(service("view_assist", "set_timer",
                          {{"entity_id", config_.view_assist}, {"type", "timer"}, {"time", words}}),
                  Job::start_timer))
    {
        toast("Not connected; nothing was set", true, 5000);
        return;
    }
    busy_minutes_ = minutes;
    busy_until_ = now() + 10000;
    toast("Timer set for " + words);
}
// A running timer cancels at once, with Undo; alarms, reminders and paused timers can't come back
// that way, so they ask for a second press (DESIGN.md, Actions 6).
void Application::cancel_timer(const std::string &id)
{
    const auto timers =
        view_assist_timers(snapshot_.states.get(config_.view_assist), now(), clock_.zone);
    auto it =
        std::find_if(timers.begin(), timers.end(), [&](const Timer &t) { return t.id == id; });
    if (it == timers.end())
        return;
    const auto &t = *it;
    const bool running = t.kind == "timer" && !t.paused;
    if (!running && armed_timer_ != id)
    {
        armed_timer_ = id;
        armed_until_ = now() + kArmMs;
        return;
    }
    armed_timer_.clear();
    const auto label = capitalised(t.name.empty() ? t.kind == "timer"   ? "Timer"
                                                    : t.kind == "alarm" ? "Alarm"
                                                                        : "Reminder"
                                                  : t.name);
    if (!dispatch(service("view_assist", "cancel_timer", {{"timer_id", t.id}}), Job::cancel_timer))
    {
        toast("Not connected; nothing was cancelled", true, 5000);
        return;
    }
    std::optional<Request> undo;
    if (running && t.left > 0)
    {
        Json data{
            {"entity_id", config_.view_assist}, {"type", "timer"}, {"time", undo_time(t.left)}};
        if (!t.name.empty())
            data["name"] = t.name;
        undo = service("view_assist", "set_timer", std::move(data));
    }
    if (undo)
        toast(label + " cancelled", false, kUndoMs, std::move(undo), label + " back on");
    else
        toast(label + " cancelled");
}
bool Application::read_only() const
{
    return !config_.read_only.empty() && snapshot_.states.get(config_.read_only).state == "on";
}
void Application::update_help(const hui::InputFrame &input)
{
    const auto &cats = config_.help();
    if (cats.empty())
        return;
    help_category_ = std::clamp(help_category_, 0, static_cast<int>(cats.size()) - 1);
    const int examples =
        static_cast<int>(cats[static_cast<std::size_t>(help_category_)].examples.size());
    if (input.is_pressed(Action::north))
    {
        if (read_only())
            toast("Visitors mode is on, so nothing runs from here", true, 4000);
        else
            request_keyboard(5);
        return;
    }
    if (input.nav == Direction::left)
        help_column_ = 0;
    else if (input.nav == Direction::right && examples)
        help_column_ = 1;
    const int move = step(input.nav, Direction::down, Direction::up);
    if (help_column_ == 0)
    {
        // Moving through the categories shows each one, as tabs do.
        const int next = std::clamp(help_category_ + move, 0, static_cast<int>(cats.size()) - 1);
        if (next != help_category_)
        {
            help_category_ = next;
            help_example_ = 0;
            armed_example_ = -1;
        }
        // The category may have just changed: enter it only if it has examples.
        if (input.is_pressed(Action::confirm) &&
            !cats[static_cast<std::size_t>(help_category_)].examples.empty())
            help_column_ = 1;
        return;
    }
    if (move)
    {
        help_example_ = std::clamp(help_example_ + move, 0, std::max(0, examples - 1));
        armed_example_ = -1;
    }
    if (input.is_pressed(Action::confirm))
        try_example(help_example_);
}
void Application::try_example(int index)
{
    const auto &cat = config_.help()[static_cast<std::size_t>(help_category_)];
    if (index < 0 || index >= static_cast<int>(cat.examples.size()) || asking_ || read_only())
        return;
    const auto &ex = cat.examples[static_cast<std::size_t>(index)];
    if (!ex.tap)
        return;
    if (ex.confirm && armed_example_ != index)
    {
        armed_example_ = index;
        armed_example_until_ = now() + kHelpArmMs;
        return;
    }
    armed_example_ = -1;
    ask_assistant(fill_room(ex.run.empty() ? ex.say : ex.run, config_.room),
                  ex.agent.empty() ? config_.agent : ex.agent, ex.agent.empty());
    if (asking_)
        asking_example_ = index;
}
// A follow-up within 90 seconds continues the same conversation ("another ten minutes").
void Application::ask_assistant(const std::string &words, const std::string &agent, bool follow_up)
{
    // Checked as the question goes: visitors mode may have come on while the keyboard was open.
    if (read_only())
    {
        toast("Visitors mode is on, so nothing runs from here", true, 4000);
        return;
    }
    const bool continuing =
        follow_up && !conversation_.empty() && now() - conversation_at_ < kFollowUpMs;
    asking_ = dispatch(ask(words, agent, continuing ? conversation_ : ""), Job::ask,
                       follow_up ? "" : "other-agent");
    if (!asking_)
        toast("Not connected to Home Assistant", true, 5000);
}
void Application::update_info(const hui::InputFrame &input)
{
    info_scroll_ = std::max(0, info_scroll_ + step(input.nav, Direction::down, Direction::up));
    // Keep: View Assist's hold mode keeps the answer on its own screen too.
    if (input.is_pressed(Action::west) && !config_.view_assist.empty() && !va_message_.empty() &&
        answer_ == va_message_)
    {
        const bool kept =
            text(snapshot_.states.get(config_.view_assist).attributes, "mode") == "hold";
        if (!dispatch(
                service("view_assist", "set_state",
                        {{"entity_id", config_.view_assist}, {"mode", kept ? "normal" : "hold"}}),
                Job::keep))
            toast("Not connected to Home Assistant", true, 5000);
    }
}
} // namespace lamplight
namespace lamplight
{
namespace
{
// The nearest rectangle in the direction pressed: along the direction counts once, across it
// twice, so moving right picks the thing beside you rather than one a row away.
int spatial_next(const std::vector<hui::gfx::Rect> &rects, int from, Direction nav)
{
    if (from < 0 || from >= static_cast<int>(rects.size()) || nav == Direction::none)
        return from;
    const auto &a = rects[static_cast<std::size_t>(from)];
    int best = from;
    float best_cost = 1e9f;
    for (int i = 0; i < static_cast<int>(rects.size()); ++i)
    {
        if (i == from)
            continue;
        const auto &b = rects[static_cast<std::size_t>(i)];
        const float dx = b.cx() - a.cx(), dy = b.cy() - a.cy();
        float along = 0, across = 0;
        if (nav == Direction::right)
            along = dx, across = dy;
        else if (nav == Direction::left)
            along = -dx, across = dy;
        else if (nav == Direction::down)
            along = dy, across = dx;
        else
            along = -dy, across = dx;
        if (along <= 1)
            continue;
        const float cost = along + 2 * (across < 0 ? -across : across);
        if (cost < best_cost)
        {
            best_cost = cost;
            best = i;
        }
    }
    return best;
}
} // namespace
void Application::update_room(const hui::InputFrame &input)
{
    const auto slots = room_slots();
    if (slots.empty())
        return;
    room_focus_ = std::clamp(room_focus_, 0, static_cast<int>(slots.size()) - 1);
    if (input.nav != Direction::none)
    {
        std::vector<hui::gfx::Rect> rects;
        for (const auto &s : slots)
            rects.push_back(s.rect);
        const int next = spatial_next(rects, room_focus_, input.nav);
        if (next != room_focus_)
            room_armed_ = -1;
        room_focus_ = next;
    }
    if (input.is_pressed(Action::confirm))
        press_room(slots[static_cast<std::size_t>(room_focus_)]);
}
void Application::press_room(const RoomSlot &slot)
{
    const auto &room = config_.room_page;
    std::optional<Request> request;
    std::string entity, label;
    bool confirm = false;
    if (slot.kind == RoomSlot::Kind::play_pause || slot.kind == RoomSlot::Kind::next)
    {
        entity = room.media;
        request =
            service("media_player",
                    slot.kind == RoomSlot::Kind::next ? "media_next_track" : "media_play_pause",
                    Json::object(), entity);
    }
    else
    {
        const auto &list = slot.kind == RoomSlot::Kind::action ? room.actions : room.tiles;
        if (slot.index < 0 || slot.index >= static_cast<int>(list.size()))
            return;
        const auto &thing = list[static_cast<std::size_t>(slot.index)];
        entity = thing.entity;
        label = thing.name.empty() ? snapshot_.states.get(entity).name(entity) : thing.name;
        confirm = thing.confirm;
        request = press_action(entity);
    }
    // A press does what the control shows: nothing while it is sending, unavailable, or (Next)
    // with nothing playing.
    const auto &state = snapshot_.states.get(entity);
    if (!request || room_busy_ == entity || !state.available() ||
        (slot.kind == RoomSlot::Kind::next && state.state != "playing"))
        return;
    // The two-press arm: the first press turns it red for five seconds.
    if (confirm && room_armed_ != room_focus_)
    {
        room_armed_ = room_focus_;
        room_armed_until_ = now() + 5000;
        return;
    }
    room_armed_ = -1;
    if (!dispatch(std::move(*request), Job::room))
    {
        toast("Not connected to Home Assistant", true, 5000);
        return;
    }
    room_busy_ = entity;
    room_busy_until_ = now() + 8000;
    if (slot.kind == RoomSlot::Kind::action)
        toast(label);
}
} // namespace lamplight
