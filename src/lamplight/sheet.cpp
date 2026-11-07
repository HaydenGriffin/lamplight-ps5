// Lamplight for PS5 - A thing's sheet: Triangle, or Cross held, opens its sliders and choices.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include "lamplight/room.hpp"
#include "ui/glyphs.hpp"
#include <algorithm>
#include <cmath>

namespace lamplight
{
using hui::Action;
using hui::Direction;
using hui::gfx::Align;
using hui::ui::Button;
namespace
{
constexpr std::int64_t kSendEveryMs = 300, kSettleMs = 800, kHoldLevelMs = 3000, kArmMs = 5000;
constexpr double kStickRangePerS = 0.5;
constexpr float kGlideFrom = 0.3f;
constexpr float kSheetW = 1180, kSheetBottom = 968, kSheetTop = 44, kHeaderH = 148;
constexpr float kChartH = 340;
constexpr float kPillRowH = 164; // room under the pills for the focus ring
constexpr int kMostUndoLamps = 64;
constexpr std::int64_t kMoveMs = 8000, kUndoMs = 8000;
constexpr std::int64_t kChartEveryMs = 5 * 60000, kChartRetryMs = 60000;
constexpr float kSheetFoot = 28, kSheetChevronFoot = 48, kHeadingH = 52;
constexpr float kCutRowMinH = 40, kCutRowFadeH = 64;
constexpr float kInset = 40, kPillH = 72, kPillGap = 14, kSwatchR = 32;
constexpr Color kWarmWhite = Color::rgb(0xffb46b), kCoolWhite = Color::rgb(0xcfe4ff);
constexpr const char *kAllLamps = "all", *kEachLamp = "Each lamp";

float control_height(const Control &row)
{
    switch (row.kind)
    {
    case Control::Kind::power:
        return row.detail.empty() ? 96 : 116;
    case Control::Kind::slider:
        return 124;
    case Control::Kind::buttons:
        return row.label.empty() ? 104 : kPillRowH;
    case Control::Kind::options:
        return kPillRowH;
    case Control::Kind::swatches:
        return 152;
    case Control::Kind::chart:
        return kChartH;
    }
    return 96;
}
float row_height(const Control &row)
{
    return control_height(row) + (row.heading.empty() ? 0 : kHeadingH);
}
bool focusable(const Control &row)
{
    return row.kind != Control::Kind::chart;
}
// Where a sheet opens: on the first level, which is what it was opened for; Power waits a row
// above, so a stray Cross doesn't turn the thing off. Without one, on the first thing that takes
// the focus.
int first_level(const std::vector<Control> &rows)
{
    auto it = std::find_if(rows.begin(), rows.end(), [](const Control &row)
                           { return row.kind == Control::Kind::slider && !row.disabled; });
    if (it == rows.end())
        it = std::find_if(rows.begin(), rows.end(), focusable);
    return it == rows.end() ? 0 : static_cast<int>(it - rows.begin());
}
// The Speakers sheet opens on the first thing that can run now, but never on Move here: one
// stray press would take the music from someone else's room.
int first_ready(const std::vector<Control> &rows)
{
    const auto it = std::find_if(rows.begin(), rows.end(),
                                 [](const Control &row)
                                 {
                                     if (row.id == "move")
                                         return false;
                                     return row.kind == Control::Kind::slider
                                                ? !row.disabled
                                                : focusable(row) && (row.options.empty() ||
                                                                     !row.options[0].disabled);
                                 });
    return it == rows.end() ? 0 : static_cast<int>(it - rows.begin());
}
// The rows can change under the focus (a row of music elsewhere goes when it stops).
int settled_focus(const std::vector<Control> &rows, int focus)
{
    focus = std::clamp(focus, 0, static_cast<int>(rows.size()) - 1);
    for (int i = focus; i < static_cast<int>(rows.size()); ++i)
        if (focusable(rows[static_cast<std::size_t>(i)]))
            return i;
    for (int i = focus; i >= 0; --i)
        if (focusable(rows[static_cast<std::size_t>(i)]))
            return i;
    return focus;
}
float fraction_of(const Control &row, double value)
{
    // A reading can sit outside the slider's range (a room warmer than the thermostat's top).
    return row.max > row.min
               ? std::clamp(static_cast<float>((value - row.min) / (row.max - row.min)), 0.0f, 1.0f)
               : 0.0f;
}
Color swatch_colour(std::uint32_t rgb)
{
    return Color::rgb(rgb);
}
// A lamp for the room-wide sheet: not one whose tile asks for a second press.
bool is_free_lamp(const RoomThing &thing)
{
    return domain_of(thing.entity) == "light" && !thing.confirm;
}
// Off or Stop beats a level still waiting to go, which would turn it on or start it moving again.
bool overrides_levels(const Control &row)
{
    return (row.kind == Control::Kind::power && row.id == "power") ||
           (row.kind == Control::Kind::buttons && row.id != "transport") || row.id == "scene";
}
// The stick pushed over, not a diagonal on the way to another row or a controller on the sofa.
bool gliding(const hui::InputFrame &input)
{
    const float over = std::fabs(input.stick_x);
    return over >= kGlideFrom && over > std::fabs(input.stick_y);
}
// The focused row near the middle, starting on a row's top so only whole rows show.
float sheet_scroll(const std::vector<Control> &rows, int focus, float content, float view_h)
{
    float above = 0;
    for (int i = 0; i < focus; ++i)
        above += row_height(rows[static_cast<std::size_t>(i)]);
    // It stays where it is while the focused row is in view: a chart at the top isn't scrolled
    // away.
    if (above + row_height(rows[static_cast<std::size_t>(focus)]) <= view_h)
        return 0;
    const float wanted =
        std::clamp(above + row_height(rows[static_cast<std::size_t>(focus)]) / 2 - view_h / 2, 0.0f,
                   std::max(0.0f, content - view_h));
    float top = 0;
    for (const auto &row : rows)
    {
        if (top >= wanted - 0.5f)
            break;
        top += row_height(row);
    }
    return top;
}
} // namespace

bool Application::open_sheet(const std::string &entity, const std::string &name,
                             const std::string &icon)
{
    const auto &state = snapshot_.states.get(entity);
    if (!state.available() || controls_for(entity, state).empty())
        return false;
    Sheet sheet;
    sheet.entity = entity;
    sheet.title = name.empty() ? state.name(entity) : name;
    sheet.icon = icon.empty() ? std::string(default_icon(entity, state)) : icon;
    sheet_ = std::move(sheet);
    sheet_->focus = first_level(sheet_rows());
    return true;
}
bool Application::open_slot_sheet(const RoomSlot &slot)
{
    if (slot.kind == RoomSlot::Kind::action)
        return false;
    // A speaker playing with its group is the group, as its Pause on the page is.
    const bool speaker =
        slot.kind == RoomSlot::Kind::play_pause || slot.kind == RoomSlot::Kind::next;
    const auto entity =
        speaker ? music_session(config_.speakers, snapshot_.states, current_room().media).target
                : slot_entity(slot);
    if (!open_sheet(entity, slot_name(slot), slot_icon(slot)))
        return false;
    sheet_->guarded = slot_confirms(slot);
    const auto &tiles = current_room().tiles;
    if (slot.kind == RoomSlot::Kind::tile && slot.index >= 0 &&
        slot.index < static_cast<int>(tiles.size()))
    {
        sheet_->sensor = tiles[static_cast<std::size_t>(slot.index)].sensor;
        sheet_->boosts = tiles[static_cast<std::size_t>(slot.index)].boosts;
    }
    const auto rows = sheet_rows();
    if (rows.empty())
    {
        sheet_.reset();
        return false;
    }
    sheet_->focus = first_level(rows);
    return true;
}
bool Application::room_has_lamps() const
{
    const auto &tiles = current_room().tiles;
    return std::any_of(tiles.begin(), tiles.end(), is_free_lamp);
}
bool Application::open_lamps_sheet()
{
    if (!room_has_lamps())
        return false;
    Sheet sheet;
    sheet.title = room_title() + " lamps";
    sheet.icon = "lightbulb-group";
    sheet.kind = SheetKind::lamps;
    sheet_ = std::move(sheet);
    sheet_->focus = first_level(sheet_rows());
    return true;
}
bool Application::open_house_sheet()
{
    if (config_.house.empty())
        return false;
    Sheet sheet;
    sheet.title = "House controls";
    sheet.icon = "home-outline";
    sheet.kind = SheetKind::house;
    sheet_ = std::move(sheet);
    // On the modes: the whole-house actions are one deliberate move up from there.
    const auto rows = sheet_rows();
    const auto mode =
        std::find_if(rows.begin(), rows.end(), [](const Control &row) { return row.id == "mode"; });
    sheet_->focus = mode == rows.end() ? 0 : static_cast<int>(mode - rows.begin());
    return true;
}
bool Application::has_speakers_sheet() const
{
    const auto &speakers = config_.speakers;
    return !speakers.group.empty() || !speakers.sleep_script.empty() ||
           speaker_list(config_).size() > 1;
}
bool Application::open_speakers_sheet()
{
    if (!has_speakers_sheet() || music_speaker().empty())
        return false;
    Sheet sheet;
    sheet.title = "Speakers";
    sheet.icon = "speaker-multiple";
    sheet.kind = SheetKind::speakers;
    sheet_ = std::move(sheet);
    sheet_->focus = first_ready(sheet_rows());
    return true;
}
void Application::track_groups()
{
    const auto before = group_lamps_.size();
    for (const auto &room : config_.rooms)
        for (const auto &tile : room.tiles)
            for (auto &lamp : group_members(tile.entity, snapshot_.states.get(tile.entity)))
                group_lamps_.insert(std::move(lamp));
    // Down through the groups inside the group Undo restores, as each one's lamps become known.
    const auto before_house = house_lamps_.size();
    if (!config_.house.undo_lights.empty())
    {
        std::vector<std::string> walk{config_.house.undo_lights};
        std::set<std::string> seen;
        while (!walk.empty() && seen.size() < kMostUndoLamps)
        {
            const auto id = std::move(walk.back());
            walk.pop_back();
            if (!seen.insert(id).second)
                continue;
            house_lamps_.insert(id);
            for (auto &lamp : strings(field(snapshot_.states.get(id).attributes, "entity_id")))
                if (domain_of(lamp) == "light")
                    walk.push_back(std::move(lamp));
        }
    }
    // Only ever grows, so a moment without states can't start a round of subscribing.
    if ((group_lamps_.size() != before || house_lamps_.size() != before_house) && watch)
        watch(config_, watched());
}
std::vector<std::string> Application::undo_lamps() const
{
    std::vector<std::string> lamps;
    for (const auto &id : house_lamps_)
    {
        const auto &state = snapshot_.states.get(id);
        if ((state.state == "on" || state.state == "off") &&
            !field(state.attributes, "entity_id").is_array())
            lamps.push_back(id);
    }
    return lamps;
}
std::vector<Control> Application::sheet_rows() const
{
    std::vector<Control> rows;
    if (!sheet_)
        return rows;
    if (sheet_->kind == SheetKind::house)
        return house_rows(config_.house, snapshot_.states);
    if (sheet_->kind == SheetKind::speakers)
        return speaker_rows(config_, snapshot_.states, music_speaker(), speaker_move_.has_value(),
                            now(), clock_.zone);
    if (sheet_->kind == SheetKind::lamps)
    {
        // A group whose lamps are tiles too is only those lamps again, so it isn't counted twice.
        const auto &tiles = current_room().tiles;
        auto is_tile = [&](const std::string &lamp)
        {
            return std::any_of(tiles.begin(), tiles.end(),
                               [&](const RoomThing &t) { return t.entity == lamp; });
        };
        for (const auto &tile : tiles)
            if (is_free_lamp(tile))
            {
                const auto &state = snapshot_.states.get(tile.entity);
                const auto members = group_members(tile.entity, state);
                if (!members.empty() && std::all_of(members.begin(), members.end(), is_tile))
                    continue;
                if (state.available())
                    rows.push_back(
                        lamp_control(tile.entity, state,
                                     tile.name.empty() ? state.name(tile.entity) : tile.name));
            }
        add_all_lamps(rows);
        if (auto scenes = scene_row(current_room(), snapshot_.states))
            rows.insert(rows.begin(), std::move(*scenes));
        return rows;
    }
    return thing_rows();
}
// One thing's own rows, then what its tile adds: its chart's sensor, its quick buttons, the room's
// scenes on its main light, and the way to the other speakers.
std::vector<Control> Application::thing_rows() const
{
    std::vector<Control> rows;
    const auto &state = snapshot_.states.get(sheet_->entity);
    if (!state.available())
        return rows;
    rows = controls_for(sheet_->entity, state);
    if (!sheet_->sensor.empty())
    {
        const auto &sensor = snapshot_.states.get(sheet_->sensor);
        for (auto &row : rows)
            if (row.kind == Control::Kind::chart)
            {
                row.entity = sheet_->sensor;
                row.attribute.clear();
                row.unit = text(sensor.attributes, "unit_of_measurement", row.unit);
            }
    }
    // The house's plan sets this radiator, so its own target and mode would only be overruled
    // (and its target is the plan's 25°, not the room's 21°): the warm-ups are the way to change
    // it.
    if (plan_runs(sheet_->entity, sheet_->boosts))
    {
        std::erase_if(rows, [](const Control &row) { return row.kind != Control::Kind::chart; });
        for (auto &row : rows)
            row.target = std::nan("");
    }
    const auto after_chart = std::find_if(rows.begin(), rows.end(), focusable);
    if (auto boosts = boost_row(sheet_->entity, sheet_->boosts))
        rows.insert(after_chart, std::move(*boosts));
    if (domain_of(sheet_->entity) == "light" && sheet_->entity == room_light(current_room()))
        if (auto scenes = scene_row(current_room(), snapshot_.states))
            rows.insert(rows.begin() + (rows.empty() ? 0 : 1), std::move(*scenes));
    if (domain_of(sheet_->entity) == "media_player" && has_speakers_sheet())
    {
        Control open;
        open.kind = Control::Kind::buttons;
        open.id = "open_speakers";
        ControlOption speakers;
        speakers.value = "speakers";
        speakers.label = "Speakers";
        speakers.icon = "speaker-multiple";
        open.options.push_back(std::move(speakers));
        rows.insert(rows.begin() + (rows.empty() ? 0 : 1), std::move(open));
    }
    // A group's lamps, each with its own level, as the tablets' sheet.
    const auto whole_group = rows.size();
    for (const auto &lamp : group_members(sheet_->entity, state))
    {
        const auto &member = snapshot_.states.get(lamp);
        if (member.available())
            rows.push_back(lamp_control(lamp, member, member.name(lamp)));
    }
    if (rows.size() > whole_group)
        rows[whole_group].heading = kEachLamp;
    // A level can't ask for a second press, so a tile that does has none here.
    if (sheet_->guarded)
        std::erase_if(rows, [](const Control &row) { return row.kind == Control::Kind::slider; });
    return rows;
}
// Two lamps or more that dim get one slider above them, starting from their average.
void Application::add_all_lamps(std::vector<Control> &rows) const
{
    int dimming = 0;
    double total = 0;
    for (const auto &row : rows)
        if (row.id == "brightness")
        {
            ++dimming;
            total += shown_value(row);
        }
    if (dimming < 2)
        return;
    Control all;
    all.kind = Control::Kind::slider;
    all.id = kAllLamps;
    all.label = "All lamps";
    all.value = total / dimming;
    rows.front().heading = kEachLamp;
    rows.insert(rows.begin(), std::move(all));
}
void Application::adjust_row(const std::vector<Control> &rows, const Control &row, double value)
{
    if (row.id != kAllLamps)
    {
        adjust(row, value);
        return;
    }
    for (const auto &lamp : rows)
        if (lamp.id == "brightness")
            adjust(lamp, value);
}
int Application::sheet_cursor(const Control &row) const
{
    if (sheet_)
        if (const auto it = sheet_->cursors.find(row.key()); it != sheet_->cursors.end())
            return std::clamp(it->second, 0, std::max(0, static_cast<int>(row.options.size()) - 1));
    return row.selected >= 0 ? row.selected : 0;
}
void Application::update_sheet(const hui::InputFrame &input)
{
    // With a second press waiting, Circle backs out of that, not out of the sheet.
    if (input.is_pressed(Action::back) && !sheet_->armed.empty())
    {
        sheet_->armed.clear();
        return;
    }
    // Triangle closes what it opens, and so do L2 and Square theirs.
    if (input.is_pressed(Action::back) || input.is_pressed(Action::north) ||
        (sheet_->kind == SheetKind::house && input.is_pressed(Action::jump_prev)) ||
        (sheet_->kind == SheetKind::speakers && input.is_pressed(Action::west)))
    {
        sheet_.reset();
        return;
    }
    const auto rows = sheet_rows();
    if (rows.empty())
    {
        sheet_.reset();
        return;
    }
    auto &sheet = *sheet_;
    sheet.focus = settled_focus(rows, sheet.focus);
    if (input.nav == Direction::up || input.nav == Direction::down)
    {
        const int by = input.nav == Direction::down ? 1 : -1;
        int next = sheet.focus + by;
        while (next >= 0 && next < static_cast<int>(rows.size()) &&
               !focusable(rows[static_cast<std::size_t>(next)]))
            next += by;
        if (next >= 0 && next < static_cast<int>(rows.size()) && next != sheet.focus)
        {
            sheet.armed.clear();
            sheet.focus = next;
        }
    }
    const auto &row = rows[static_cast<std::size_t>(sheet.focus)];
    const int across = input.nav == Direction::right ? 1 : input.nav == Direction::left ? -1 : 0;
    switch (row.kind)
    {
    case Control::Kind::slider:
        if (row.disabled)
            break;
        // The D-pad steps; the left stick glides, further over for faster.
        if (across && !input.nav_from_stick)
            adjust_row(rows, row, snapped(row, shown_value(row) + across * row.step));
        else if (gliding(input))
            adjust_row(rows, row,
                       shown_value(row) + static_cast<double>(input.stick_x) * (row.max - row.min) *
                                              kStickRangePerS * static_cast<double>(frame_dt_));
        // The right stick, as on the room's tiles: up is more.
        else if (std::fabs(input.stick2_y) >= 0.2f)
            adjust_row(rows, row,
                       shown_value(row) - static_cast<double>(input.stick2_y) *
                                              (row.max - row.min) * kStickRangePerS *
                                              static_cast<double>(frame_dt_));
        break;
    case Control::Kind::options:
    case Control::Kind::swatches:
    case Control::Kind::buttons:
        if (across)
        {
            sheet.cursors[row.key()] =
                std::clamp(sheet_cursor(row) + across, 0, static_cast<int>(row.options.size()) - 1);
            sheet.armed.clear();
        }
        break;
    case Control::Kind::power:
    case Control::Kind::chart:
        break;
    }
    if (input.is_pressed(Action::confirm))
        press_sheet_row(row);
}
void Application::press_sheet_row(const Control &row)
{
    if (row.kind == Control::Kind::chart)
        return;
    if (row.kind == Control::Kind::slider)
    {
        if (row.press && !row.disabled)
            run_called(*row.press, row.press_done);
        return;
    }
    const bool power = row.kind == Control::Kind::power;
    const int at = sheet_cursor(row);
    if (!power && at >= static_cast<int>(row.options.size()))
        return;
    const auto *option = power ? nullptr : &row.options[static_cast<std::size_t>(at)];
    if (option && option->disabled)
        return;
    if (row.id == "open_speakers")
    {
        open_speakers_sheet();
        return;
    }
    // Visitors can't run the house from here, but the way out of visitors mode stays.
    if (sheet_->kind == SheetKind::house && read_only() && row.entity != config_.read_only)
    {
        toast("Visitors mode is on, so nothing runs from here", true, 4000);
        return;
    }
    // Anything that can't be taken back (Unlock), and everything on a tile that asks.
    const bool asks = sheet_->guarded || (power ? row.asks : option->asks);
    if (asks && sheet_->armed != row.key())
    {
        sheet_->armed = row.key();
        sheet_->armed_until = now() + kArmMs;
        return;
    }
    sheet_->armed.clear();
    if (option && option->undo_lights)
    {
        if (!waiting_action_)
            run_noting_lamps(*option);
        sheet_.reset();
        return;
    }
    auto request = power ? std::optional<Request>(power_request(row)) : option_request(row, at);
    if (!request)
        return;
    const auto &done = power ? row.press_done : option->done;
    if (option && !option->moves_to.empty())
    {
        // Said once it plays there: Music Assistant can accept a move and then drop it.
        if (!dispatch(std::move(*request), Job::said))
        {
            toast("Not connected to Home Assistant", true, 5000);
            return;
        }
        const auto now_there = music_session(config_.speakers, snapshot_.states, option->moves_to);
        speaker_move_ = SpeakerMove{option->moves_to,
                                    done,
                                    now() + kMoveMs,
                                    now_there.target,
                                    text(now_there.state.attributes, "media_title"),
                                    now_there.state.state == "playing"};
        return;
    }
    run_called(std::move(*request), done);
    if (overrides_levels(row))
        drop_overridden_settings(row);
    if (option && option->closes)
        sheet_.reset();
}
void Application::run_called(Request request, const std::string &done)
{
    if (!dispatch(std::move(request), done.empty() ? Job::setting : Job::said, done))
        toast("Not connected to Home Assistant", true, 5000);
}
void Application::run_noting_lamps(const ControlOption &option)
{
    const auto lamps = undo_lamps();
    if (!lamps.empty() &&
        dispatch(
            service("scene", "create", {{"scene_id", kUndoSceneId}, {"snapshot_entities", lamps}}),
            Job::snapshot))
    {
        waiting_action_ = option;
        return;
    }
    finish_house_action(option, false);
}
void Application::finish_house_action(const ControlOption &option, bool can_undo)
{
    if (!option.press ||
        !dispatch(*option.press, can_undo ? Job::undoable : Job::setting, option.done))
    {
        toast("Not connected to Home Assistant", true, 5000);
        return;
    }
    toast(option.done);
}
// Only once it has finished: a script that takes its time would outlast the offer, or switch the
// lamps off again after an early Undo.
void Application::offer_lamps_undo(const std::string &done)
{
    toast(done, false, kUndoMs,
          service("scene", "turn_on", Json::object(), std::string("scene.") + kUndoSceneId),
          "Lights back as they were");
}
void Application::check_speaker_move()
{
    if (!speaker_move_)
        return;
    const auto session = music_session(config_.speakers, snapshot_.states, speaker_move_->target);
    const bool arrived = !speaker_move_->was_playing ||
                         session.target != speaker_move_->was_session ||
                         text(session.state.attributes, "media_title") != speaker_move_->was_title;
    if (session.state.state == "playing" && arrived)
    {
        toast(speaker_move_->done);
        speaker_move_.reset();
    }
    else if (now() > speaker_move_->until)
    {
        toast("Music Assistant couldn't move the music. Try again in a moment", true, 5000);
        speaker_move_.reset();
    }
}
void Application::update_chart()
{
    if (!sheet_ || sheet_->kind != SheetKind::thing)
        return;
    const auto rows = sheet_rows();
    const auto row = std::find_if(rows.begin(), rows.end(),
                                  [](const Control &r) { return r.kind == Control::Kind::chart; });
    if (row == rows.end())
        return;
    if (chart_.entity != row->entity)
    {
        chart_ = {};
        chart_.entity = row->entity;
    }
    if (chart_.loading || !ready() || (chart_.at && now() - chart_.at < kChartEveryMs) ||
        (chart_.failed && now() - chart_.asked < kChartRetryMs))
        return;
    chart_.asked = now();
    chart_.loading = dispatch(history_request(row->entity, row->attribute, now(), clock_.zone),
                              Job::history, row->entity) != 0;
    chart_.failed = !chart_.loading;
}
void Application::drop_settings(const std::string &entity)
{
    std::erase_if(settings_, [&](const auto &s) { return s.second.control.entity == entity; });
}
// A light's Off or scene reaches the lamps under it too, whose own levels would turn them back on.
void Application::drop_overridden_settings(const Control &row)
{
    drop_settings(row.entity);
    if (row.id != "power" && row.id != "scene")
        return;
    for (const auto &lamp : group_members(row.entity, snapshot_.states.get(row.entity)))
        drop_settings(lamp);
    if (sheet_ && sheet_->kind == SheetKind::lamps)
        for (const auto &other : sheet_rows())
            drop_settings(other.entity);
}
void Application::adjust(const Control &control, double value)
{
    auto [it, fresh] = settings_.try_emplace(control.key());
    auto &setting = it->second;
    if (fresh)
        setting.sent = snapped(control, control.value);
    setting.control = control;
    setting.value = std::clamp(value, control.min, std::max(control.min, control.max));
    setting.touched_at = now();
}
double Application::shown_value(const Control &control) const
{
    const auto it = settings_.find(control.key());
    return it == settings_.end() ? control.value : it->second.value;
}
double Application::current_level(const Control &control) const
{
    const auto &state = snapshot_.states.get(control.entity);
    for (const auto &row : controls_for(control.entity, state))
        if (row.id == control.id)
            return row.value;
    return std::nan("");
}
// Both ends of a thermostat's range go together, the other as it stands now: it may have moved
// since this one did.
Control Application::with_latest_other_end(Control control) const
{
    if (control.id != "temp_low" && control.id != "temp_high")
        return control;
    const auto other =
        settings_.find(control.entity + (control.id == "temp_low" ? "/temp_high" : "/temp_low"));
    if (other != settings_.end())
        control.other_end = snapped(other->second.control, other->second.value);
    return control;
}
// Each level being set goes to Home Assistant once per new value (at most every 300 ms), from any
// page, so closing the sheet doesn't lose the last step. It shows until Home Assistant agrees, or
// for three seconds after the last send.
void Application::send_settings()
{
    for (auto it = settings_.begin(); it != settings_.end();)
    {
        auto &s = it->second;
        const double target = snapped(s.control, s.value);
        if (std::fabs(s.sent - target) < 1e-6)
        {
            const double level = current_level(s.control);
            const bool agrees =
                !std::isnan(level) && std::fabs(level - target) <= s.control.step / 2;
            const bool settled = now() - s.touched_at > kSettleMs;
            if (settled && (agrees || now() - s.sent_at > kHoldLevelMs))
                it = settings_.erase(it);
            else
                ++it;
            continue;
        }
        if (now() - s.sent_at < kSendEveryMs)
        {
            ++it;
            continue;
        }
        auto request = slider_request(with_latest_other_end(s.control), target);
        if (!request || !dispatch(std::move(*request), Job::setting))
        {
            toast("Not connected to Home Assistant", true, 5000);
            it = settings_.erase(it);
            continue;
        }
        s.sent = target;
        s.sent_at = now();
        ++it;
    }
}
void Application::draw_sheet_row(Paint &p, const Control &row, const Rect &r, bool focused) const
{
    if (row.kind == Control::Kind::chart)
    {
        draw_chart(p, row, r);
        return;
    }
    if (focused)
        p.list.rounded_rect(r, size::small_radius, tone::raised);
    // A switch has no thumb or pill to carry the ring, so its whole row does.
    if (focused && row.kind == Control::Kind::power)
        p.focus(r.inset(11), size::small_radius - 11);
    const float x = r.x + 28, w = r.w - 56;
    switch (row.kind)
    {
    case Control::Kind::power:
    {
        // A mode that is on is sky, not warm (The Sky Rule): it isn't a lamp.
        const bool mode = row.id == "mode";
        const auto on_colour = mode ? tone::accent : tone::warm;
        const float left = row.icon.empty() ? x : x + size::disc_small + 20;
        if (!row.icon.empty())
            p.disc(x + size::disc_small / 2, r.cy(), size::disc_small, row.icon,
                   row.on ? DiscKind::mode : DiscKind::rest);
        const float label_y = row.detail.empty() ? r.cy() + 12 : r.y + 46;
        p.text(row.label, left, label_y, size::body, tone::text, true, Align::left,
               r.x + r.w - 260 - left);
        if (!row.detail.empty())
            p.text(row.detail, left, r.y + 84, size::caption, tone::muted, false, Align::left,
                   r.x + r.w - 220 - left);
        const bool ask = sheet_ && sheet_->armed == row.key();
        p.text(ask      ? "Press again"
               : row.on ? "On"
                        : "Off",
               r.x + r.w - 160, r.cy() + 12, size::label,
               ask      ? tone::alert
               : row.on ? on_colour
                        : tone::muted,
               false, Align::right);
        const Rect track{r.x + r.w - 132, r.cy() - 25, 104, 50};
        p.list.rounded_rect(track, 25, row.on ? on_colour : tone::raised2);
        p.list.circle(row.on ? track.x + track.w - 25 : track.x + 25, track.cy(), 19,
                      row.on ? (mode ? tone::accent_ink : tone::warm_ink) : tone::text);
        break;
    }
    case Control::Kind::slider:
    {
        const double value = shown_value(row);
        const bool warmth = row.id == "warmth";
        const bool moving = settings_.contains(row.key());
        if (row.disabled)
            p.list.push_opacity(0.55f);
        const float label_w =
            p.text(row.label, x, r.y + 48, size::body, tone::text, true, Align::left, w - 360);
        if (!row.detail.empty())
            p.text(row.detail, x + label_w + 20, r.y + 48, size::caption, tone::accent, false,
                   Align::left, w - 240 - label_w);
        // At rest it reads what the thing reports; on the move, the step being sent.
        const auto words = moving ? value_words(row, value) : reading_words(row, value);
        p.text(row.disabled ? row.disabled_words : words, x + w, r.y + 48, size::label,
               moving ? tone::accent : tone::sky_muted, false, Align::right);
        const Rect track{x + 8, r.y + 68, w - 16, 44};
        const float at = fraction_of(row, moving ? snapped(row, value) : value);
        if (warmth)
            p.slider(track, at, focused, kWarmWhite, kCoolWhite);
        else
            p.slider(track, at, focused, moving ? tone::accent : tone::warm);
        if (row.disabled)
            p.list.pop_opacity();
        break;
    }
    case Control::Kind::swatches:
    {
        const int cursor = sheet_cursor(row);
        const auto &chosen = row.options[static_cast<std::size_t>(cursor)];
        p.text(row.label, x, r.y + 48, size::body, tone::text, true);
        p.text(focused             ? chosen.label
               : row.selected >= 0 ? row.options[static_cast<std::size_t>(row.selected)].label
                                   : "",
               x + w, r.y + 48, size::label, tone::sky_muted, false, Align::right);
        const float span = w - 2 * kSwatchR;
        const float gap =
            row.options.size() > 1 ? span / static_cast<float>(row.options.size() - 1) : 0;
        for (int i = 0; i < static_cast<int>(row.options.size()); ++i)
        {
            const float cx = x + kSwatchR + static_cast<float>(i) * gap, cy = r.y + 104;
            p.list.circle(cx, cy, kSwatchR,
                          swatch_colour(row.options[static_cast<std::size_t>(i)].colour));
            if (i == row.selected)
                p.list.ring(cx, cy, kSwatchR + 7, 4, tone::text);
            if (focused && i == cursor)
                p.list.ring(cx, cy, kSwatchR + 15, 5, tone::accent);
        }
        break;
    }
    case Control::Kind::options:
    case Control::Kind::buttons:
    {
        const bool labelled = !row.label.empty();
        if (labelled)
            p.text(row.label, x, r.y + 48, size::body, tone::text, true, Align::left,
                   row.detail.empty() ? w : w - 420);
        if (labelled && !row.detail.empty())
            p.text(row.detail, x + w, r.y + 48, size::label, tone::sky_muted, false, Align::right,
                   400);
        const float top = labelled ? r.y + 66 : r.cy() - kPillH / 2;
        const int cursor = sheet_cursor(row);
        const bool armed = sheet_ && sheet_->armed == row.key();
        // As many as fit, sliding along so the one in hand stays in view.
        std::vector<float> widths;
        float total = 0, before = 0;
        for (int i = 0; i < static_cast<int>(row.options.size()); ++i)
        {
            const auto &option = row.options[static_cast<std::size_t>(i)];
            const auto label = armed && i == cursor ? "Confirm " + option.label : option.label;
            // Room for the check on the chosen one, or its icon.
            const bool marked = i == row.selected || !option.icon.empty();
            const float width = std::clamp(
                p.measure(label, size::button, true) + 64 + (marked ? 44 : 0), 150.0f, 460.0f);
            if (i < cursor)
                before += width + kPillGap;
            widths.push_back(width);
            total += width + kPillGap;
        }
        const float at = widths.empty() ? 0 : widths[static_cast<std::size_t>(cursor)];
        const float slide = std::clamp(before + at / 2 - w / 2, 0.0f, std::max(0.0f, total - w));
        p.list.push_clip({r.x, r.y, r.w, r.h});
        float pill_x = x - slide;
        for (int i = 0; i < static_cast<int>(row.options.size()); ++i)
        {
            const auto &option = row.options[static_cast<std::size_t>(i)];
            const bool here = focused && i == cursor;
            const bool ask = armed && i == cursor;
            // A scene sets a light's level, so its chosen one is warm (The Lamp Rule), unless
            // it is Off.
            const bool lit_scene = row.id == "scene" && option.value != "off";
            p.pill({pill_x, top, widths[static_cast<std::size_t>(i)], kPillH},
                   i == row.selected && !ask ? "check" : option.icon,
                   ask ? "Confirm " + option.label : option.label,
                   ask                 ? PillKind::danger
                   : i != row.selected ? PillKind::rest
                   : lit_scene         ? PillKind::primary
                   : row.id == "scene" ? PillKind::rest
                                       : PillKind::selected,
                   here, option.disabled, true);
            pill_x += widths[static_cast<std::size_t>(i)] + kPillGap;
        }
        p.list.pop_clip();
        break;
    }
    case Control::Kind::chart:
        break;
    }
}
std::string Application::sheet_words(const std::vector<Control> &rows, bool &lit) const
{
    switch (sheet_->kind)
    {
    case SheetKind::lamps:
    {
        int on = 0, lamps = 0;
        for (const auto &row : rows)
            if (row.id == "brightness" || row.id == "power")
            {
                ++lamps;
                on += is_on(row.entity, snapshot_.states.get(row.entity));
            }
        lit = on > 0;
        return on ? std::to_string(on) + " of " + std::to_string(lamps) + " on"
                  : std::string("Everything's off");
    }
    case SheetKind::house:
    {
        const auto words = modes_words(config_.house, snapshot_.states);
        lit = !words.empty();
        return lit ? words : config_.house.modes.empty() ? "" : "No modes on";
    }
    case SheetKind::speakers:
    {
        auto words = speakers_words(config_, snapshot_.states);
        lit = words != "Nothing playing";
        return words;
    }
    case SheetKind::thing:
        break;
    }
    const auto &state = snapshot_.states.get(sheet_->entity);
    lit = is_on(sheet_->entity, state);
    return thing_words(sheet_->entity, state, snapshot_.states.get(sheet_->sensor),
                       plan_runs(sheet_->entity, sheet_->boosts));
}
void Application::draw_sheet(Paint &p) const
{
    if (!sheet_)
        return;
    const auto rows = sheet_rows();
    if (rows.empty())
        return;
    p.list.rounded_rect({0, 0, 1920, 1080}, 0, tone::dimmer);
    float content = 0;
    for (const auto &row : rows)
        content += row_height(row);
    const float height = std::min(kSheetBottom - kSheetTop, kHeaderH + content + 28);
    const Rect sheet{(1920 - kSheetW) / 2, kSheetBottom - height, kSheetW, height};
    p.list.shadow(sheet.inset(-6), 40, 40, Color::rgb(0x000000, 0.6f));
    p.panel(sheet, 40, tone::panel_solid);
    // The header: disc, name and how it is now; Circle closes.
    bool lit = false;
    const auto words = sheet_words(rows, lit);
    const bool house = sheet_->kind == SheetKind::house;
    p.disc(sheet.x + kInset + size::disc / 2, sheet.y + kHeaderH / 2, size::disc, sheet_->icon,
           !lit    ? DiscKind::rest
           : house ? DiscKind::mode
                   : DiscKind::on);
    const float tx = sheet.x + kInset + size::disc + 28;
    p.text(sheet_->title, tx, sheet.y + kHeaderH / 2 - 6, size::title, tone::text, true,
           Align::left, sheet.w - (tx - sheet.x) - 120);
    p.text(words, tx, sheet.y + kHeaderH / 2 + 38, size::label,
           !lit    ? tone::muted
           : house ? tone::accent
                   : tone::warm,
           false, Align::left, sheet.w - (tx - sheet.x) - 120);
    auto style = hui::ui::GlyphStyle::dark();
    hui::ui::draw_button(p.list, fonts_.kit, style, Button::circle, sheet.x + sheet.w - 88,
                         sheet.y + kHeaderH / 2, 48);
    // The rows, scrolling so the focused one stays in view.
    const bool scrolls = kHeaderH + content + kSheetFoot > kSheetBottom - kSheetTop;
    const Rect view{sheet.x, sheet.y + kHeaderH, sheet.w,
                    sheet.h - kHeaderH - (scrolls ? kSheetChevronFoot : kSheetFoot)};
    const int focus = settled_focus(rows, sheet_->focus);
    const float scroll = sheet_scroll(rows, focus, content, view.h);
    float y = view.y - scroll;
    bool more_below = false;
    std::string below; // the first heading out of sight
    float fade_top = 0;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
    {
        const auto &row = rows[static_cast<std::size_t>(i)];
        const float h = row_height(row);
        const bool fits = y + h <= view.y + view.h + 0.5f;
        // The first row that doesn't fit shows as far as it can, under a fade.
        const bool cut = !fits && !more_below && y < view.y + view.h - kCutRowMinH;
        if (!fits)
        {
            if (!cut && below.empty())
                below = row.heading;
            more_below = true;
        }
        if ((fits || cut) && y >= view.y - 0.5f)
        {
            if (cut)
            {
                p.list.push_clip(view);
                fade_top = std::max(y, view.y + view.h - kCutRowFadeH);
            }
            const float head = row.heading.empty() ? 0 : kHeadingH;
            if (head > 0)
                p.text(row.heading, sheet.x + kInset + 20, y + 40, size::caption, tone::muted,
                       true);
            draw_sheet_row(
                p, row,
                {sheet.x + kInset - 8, y + head + 4, sheet.w - 2 * kInset + 16, h - head - 8},
                i == focus && !alert_);
            if (cut)
                p.list.pop_clip();
        }
        y += h;
    }
    if (fade_top > 0)
        p.list.gradient_rect({sheet.x, fade_top, sheet.w, view.y + view.h - fade_top}, 0,
                             tone::panel_solid.with_alpha(0), tone::panel_solid);
    if (scroll > 0.5f)
        p.icon("chevron-up", sheet.x + sheet.w / 2, sheet.y + kHeaderH - 14, 36, tone::muted);
    if (more_below)
    {
        const auto word = below.empty() ? std::string("More") : below;
        const float left = sheet.x + sheet.w / 2 - (p.measure(word, size::caption) + 44) / 2;
        const float cy = sheet.y + sheet.h - kSheetChevronFoot / 2;
        p.icon("chevron-down", left + 18, cy, 36, tone::muted);
        p.text(word, left + 44, cy + 10, size::caption, tone::muted);
    }
}
} // namespace lamplight
