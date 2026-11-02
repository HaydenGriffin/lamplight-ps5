// Lamplight for PS5 - Setup, its lists of entities, and Close.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"

namespace lamplight
{
using hui::gfx::Align;
void Application::draw_setup(Paint &p) const
{
    std::string subtitle = snapshot_.ready ? "Connected to Home Assistant" : snapshot_.status;
    if (page_ == Page::pick)
        subtitle = pick_ == Pick::calendars || pick_ == Pick::appliances
                       ? "Select to turn each one on or off"
                       : "Select one";
    if (page_ == Page::quit)
        subtitle = "Lamplight keeps nothing running once it's closed";
    draw_header(p, page_title(), subtitle,
                page_ == Page::setup && !snapshot_.ready ? tone::warn : tone::muted);
    const Rect side{1200, size::content_top, 1920 - size::margin - 1200,
                    size::content_bottom - size::content_top};
    p.panel({size::margin - 16, size::content_top - 16, 1060 + 32, side.h + 32}, size::tile_radius,
            tone::panel);
    hui::ui::Canvas canvas{p.list, fonts_.kit, 0, time_};
    list_.draw(canvas);
    p.panel(side);
    std::string heading = "On this PS5", body;
    if (page_ == Page::setup)
    {
        heading = "Start here";
        body = "Enter Home Assistant's local IPv4 address and a long-lived access token from your "
               "profile, then Connect. Then choose what each page shows.\n\nSettings stay on this "
               "PS5, token and Wi-Fi password included. Local HTTP only.";
    }
    else if (page_ == Page::pick)
    {
        heading = pick_options().empty() ? "Nothing found" : "From Home Assistant";
        switch (pick_)
        {
        case Pick::weather:
            body = "The forecast on Weather and the temperature on the clock.";
            break;
        case Pick::calendars:
            body = "Agenda shows today and tomorrow from every calendar that is on.";
            break;
        case Pick::view_assist:
            body =
                "A View Assist device's timers show on Timers, and are set and cancelled through "
                "it. Its assistant's answers show on Assistant.";
            break;
        case Pick::appliances:
            body =
                "Timer helpers that set finishes_at while they run, such as a washing machine's.";
            break;
        case Pick::agent:
            body = "The conversation agent Help's examples and typed questions go to.";
            break;
        case Pick::read_only:
            body = "While this switch is on (visitors in, say), Help shows its examples but runs "
                   "nothing.";
            break;
        }
    }
    else
        body = "Close returns to the PS5 home screen. Your settings are kept.";
    p.text(heading, side.x + 40, side.y + 70, size::body, tone::text, true);
    p.paragraph(body, side.x + 40, side.y + 124, size::label, side.w - 80, 44, tone::muted, 12);
    if (page_ == Page::setup && !storage_status_.empty())
        p.text(storage_status_, side.x + 40, side.y + side.h - 40, size::caption, tone::muted,
               false, Align::left, side.w - 80);
}
} // namespace lamplight
