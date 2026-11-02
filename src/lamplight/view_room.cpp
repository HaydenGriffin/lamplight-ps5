// Lamplight for PS5 - The room page: what's playing, the room's actions, and its things as tiles.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include "lamplight/room.hpp"
#include <algorithm>

namespace lamplight
{
using hui::gfx::Align;
namespace
{
constexpr float kLeftW = 820, kGap = 24, kTop = size::content_top;
constexpr float kBottom = size::content_bottom;
constexpr float kHeroH = 404, kPillH = 84, kPillGap = 18;
} // namespace

std::string Application::room_title() const
{
    if (!config_.room_page.name.empty())
        return config_.room_page.name;
    return config_.room.empty() ? "Room" : capitalised(config_.room);
}
std::vector<Application::RoomSlot> Application::room_slots() const
{
    const auto &room = config_.room_page;
    std::vector<RoomSlot> slots;
    const float left = size::margin;
    float y = kTop;
    if (!room.media.empty())
    {
        const float by = kTop + kHeroH - kPillH - 28;
        slots.push_back({RoomSlot::Kind::play_pause, 0, {left + 32, by, 250, kPillH}});
        slots.push_back({RoomSlot::Kind::next, 0, {left + 32 + 250 + 18, by, 210, kPillH}});
        y = kTop + kHeroH + kGap;
    }
    // Actions: pills in two columns under the hero (or filling the column without one).
    const float pw = (kLeftW - kPillGap) / 2;
    for (std::size_t i = 0; i < room.actions.size(); ++i)
        slots.push_back({RoomSlot::Kind::action,
                         static_cast<int>(i),
                         {left + static_cast<float>(i % 2) * (pw + kPillGap),
                          y + static_cast<float>(i / 2) * (kPillH + kPillGap), pw, kPillH}});
    // Tiles: three across, as many rows as they need.
    const float tx = left + kLeftW + 40, tw = 1920 - size::margin - tx;
    const int rows = std::max(1, static_cast<int>((room.tiles.size() + 2) / 3));
    const float cw = (tw - 2 * kGap) / 3;
    const float ch = std::min(300.0f, (kBottom - kTop - kGap * static_cast<float>(rows - 1)) /
                                          static_cast<float>(rows));
    for (std::size_t i = 0; i < room.tiles.size(); ++i)
        slots.push_back({RoomSlot::Kind::tile,
                         static_cast<int>(i),
                         {tx + static_cast<float>(i % 3) * (cw + kGap),
                          kTop + static_cast<float>(i / 3) * (ch + kGap), cw, ch}});
    return slots;
}
void Application::draw_room(Paint &p) const
{
    const auto &room = config_.room_page;
    int on = 0, things = 0;
    for (const auto &t : room.tiles)
    {
        const auto domain = domain_of(t.entity);
        if (domain == "script" || domain == "scene" || domain == "climate")
            continue;
        ++things;
        on += is_on(t.entity, snapshot_.states.get(t.entity));
    }
    draw_header(p, room_title(),
                things ? (on ? std::to_string(on) + " of " + std::to_string(things) + " on"
                             : std::string("Everything's off"))
                       : "",
                on ? tone::warm : tone::muted);
    const auto slots = room_slots();
    // The hero: what's playing in the room.
    if (!room.media.empty())
    {
        const auto &m = snapshot_.states.get(room.media);
        const bool playing = m.state == "playing";
        const Rect hero{size::margin, kTop, kLeftW, kHeroH};
        p.panel(hero, size::tile_radius + 6, Color::rgb(0x1c1f25, 0.82f));
        if (playing)
            p.wash(hero, size::tile_radius + 6);
        p.disc(hero.x + 72, hero.y + 72, size::disc, "speaker",
               playing ? DiscKind::on : DiscKind::rest);
        p.text(m.name(room.media), hero.x + 132, hero.y + 64, size::label, tone::muted, false,
               Align::left, hero.w - 170);
        p.text(state_words(room.media, m), hero.x + 132, hero.y + 100, size::label,
               playing ? tone::warm : tone::muted);
        const auto title = text(m.attributes, "media_title");
        const auto artist = text(m.attributes, "media_artist");
        const float after =
            p.paragraph(title.empty() ? "Nothing playing" : title, hero.x + 32, hero.y + 196,
                        size::headline, hero.w - 64, 62, tone::text, 2, true);
        if (!artist.empty())
            p.text(artist, hero.x + 32, after - 10, size::body, tone::muted, false, Align::left,
                   hero.w - 64);
    }
    for (int i = 0; i < static_cast<int>(slots.size()); ++i)
    {
        const auto &slot = slots[static_cast<std::size_t>(i)];
        const bool focused = i == room_focus_ && !alert_;
        const bool armed = i == room_armed_;
        if (slot.kind == RoomSlot::Kind::play_pause || slot.kind == RoomSlot::Kind::next)
        {
            const bool playing = snapshot_.states.get(room.media).state == "playing";
            const bool busy = room_busy_ == room.media;
            if (slot.kind == RoomSlot::Kind::next)
                p.pill(slot.rect, "skip-next", "Next", PillKind::rest, focused, busy || !playing,
                       true);
            else
                // Play turns music on, so it is the warm primary; Pause is the lead neutral.
                p.pill(slot.rect, playing ? "pause" : "play", playing ? "Pause" : "Play",
                       playing ? PillKind::lead : PillKind::primary, focused, busy);
            continue;
        }
        const auto &list = slot.kind == RoomSlot::Kind::action ? room.actions : room.tiles;
        const auto &thing = list[static_cast<std::size_t>(slot.index)];
        const auto &state = snapshot_.states.get(thing.entity);
        const auto name = thing.name.empty() ? state.name(thing.entity) : thing.name;
        const auto icon = thing.icon.empty() ? std::string(default_icon(thing.entity)) : thing.icon;
        const bool busy = room_busy_ == thing.entity;
        if (slot.kind == RoomSlot::Kind::action)
        {
            p.pill(slot.rect, icon, armed ? "Press again" : name,
                   armed ? PillKind::danger : PillKind::rest, focused, busy || !state.available());
            continue;
        }
        // A tile: smoked glass; on, it glows (lamp wash, warm disc, warm state).
        const bool lit = is_on(thing.entity, state);
        const Rect &r = slot.rect;
        if (!state.available())
            p.list.push_opacity(0.55f);
        p.panel(r);
        if (lit)
            p.wash(r, size::tile_radius);
        p.disc(r.x + 28 + size::disc / 2, r.y + 28 + size::disc / 2, size::disc, icon,
               lit ? DiscKind::on : DiscKind::rest);
        p.text(name, r.x + 28, r.y + r.h - 64, size::body, tone::text, true, Align::left, r.w - 56);
        p.text(armed  ? "Press again"
               : busy ? "Sending\xE2\x80\xA6"
                      : state_words(thing.entity, state),
               r.x + 28, r.y + r.h - 26, size::label,
               armed ? tone::alert
               : lit ? tone::warm
                     : tone::muted,
               false, Align::left, r.w - 56);
        if (!state.available())
            p.list.pop_opacity();
        if (focused)
            p.focus(r, size::tile_radius);
    }
}
void Application::draw_alert(Paint &p) const
{
    // Over everything: the dimmer, then one panel. The disc glows because the thing is done
    // and ringing (on), and the live dot pulses beside the title.
    p.list.rounded_rect({0, 0, 1920, 1080}, 0, tone::dimmer);
    const Rect r{460, 250, 1000, 520};
    p.list.shadow(r.inset(-6), 48, 40, Color::rgb(0x000000, 0.6f));
    p.panel(r, 48, tone::panel_solid);
    p.wash(r, 48);
    p.disc(r.cx(), r.y + 130, size::disc_big + 24, alert_->icon, DiscKind::on);
    p.text(alert_->title, r.cx(), r.y + 300, 76, tone::text, true, Align::center, r.w - 80);
    p.text(alert_->detail, r.cx(), r.y + 360, size::title, tone::warm, false, Align::center,
           r.w - 80);
    p.pill({r.cx() - 160, r.y + r.h - 124, 320, size::target}, "close",
           alert_still_listed() ? "Stop" : "OK", PillKind::rest, true, false, true);
}
} // namespace lamplight
