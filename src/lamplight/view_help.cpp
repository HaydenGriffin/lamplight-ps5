// Lamplight for PS5 - Help: "What can I say?", with examples that run for real.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include <algorithm>

namespace lamplight
{
using hui::gfx::Align;
void Application::draw_help(Paint &p) const
{
    const bool still = read_only();
    draw_header(p, "What can I say?",
                still
                    ? "Start with “" + config_.wake_phrase + "”, then say it"
                    : "Choose an example to try it for real, or press Triangle to type a question",
                tone::muted);
    const auto &cats = config_.help();
    if (cats.empty())
        return;
    // Categories: pills down the left, the chosen one in sky (The Sky Rule).
    const float cat_h = 96, cat_gap = 14, cat_w = 500;
    const float cat_top = size::content_top;
    const int fit = static_cast<int>((size::content_bottom - cat_top) / (cat_h + cat_gap));
    const int count = static_cast<int>(cats.size());
    const int first = std::clamp(help_category_ - fit + 1, 0, std::max(0, count - fit));
    for (int i = first; i < count && i < first + fit; ++i)
    {
        const auto &c = cats[static_cast<std::size_t>(i)];
        const Rect r{size::margin, cat_top + static_cast<float>(i - first) * (cat_h + cat_gap),
                     cat_w, cat_h};
        const bool selected = i == help_category_;
        p.list.rounded_rect(r, cat_h / 2, selected ? tone::accent : tone::raised);
        const Color ink = selected ? tone::accent_ink : tone::text;
        p.icon(c.icon.empty() ? "comment-text-outline" : c.icon, r.x + 56, r.cy(), 44, ink);
        p.text(c.name, r.x + 100, r.cy() + 12, size::body, ink, true, Align::left, r.w - 130);
        if (selected && help_column_ == 0)
            p.focus(r, cat_h / 2);
    }
    // The chosen category's examples, each starting with the wake phrase.
    const auto &cat = cats[static_cast<std::size_t>(help_category_)];
    const Rect panel{size::margin + cat_w + 28, size::content_top,
                     1920 - size::margin - (size::margin + cat_w + 28),
                     size::content_bottom - size::content_top};
    p.panel(panel);
    p.disc(panel.x + 70, panel.y + 70, size::disc,
           cat.icon.empty() ? "comment-text-outline" : cat.icon, DiscKind::raised2);
    p.text(cat.name, panel.x + 132, panel.y + 66, size::title, tone::text, true);
    if (!cat.hint.empty())
        p.text(cat.hint, panel.x + 132, panel.y + 106, size::label, tone::muted, false, Align::left,
               panel.w - 170);
    const float row_h = 104, row_gap = 12, top = panel.y + 148;
    const float bottom = panel.y + panel.h - (cat.tip.empty() ? 24 : 92);
    const int rows_fit =
        std::max(1, static_cast<int>((bottom - top + row_gap) / (row_h + row_gap)));
    const int examples = static_cast<int>(cat.examples.size());
    const int first_ex =
        std::clamp(help_example_ - rows_fit + 1, 0, std::max(0, examples - rows_fit));
    const auto wake = config_.wake_phrase.empty() ? std::string() : config_.wake_phrase + ",";
    for (int i = first_ex; i < examples && i < first_ex + rows_fit; ++i)
    {
        const auto &ex = cat.examples[static_cast<std::size_t>(i)];
        const Rect r{panel.x + 24, top + static_cast<float>(i - first_ex) * (row_h + row_gap),
                     panel.w - 48, row_h};
        const bool runs = ex.tap && !still;
        const bool focused = help_column_ == 1 && help_example_ == i;
        const bool armed = armed_example_ == i;
        const bool asking = asking_ && asking_example_ == i;
        if (runs)
            p.list.rounded_rect(r, size::small_radius,
                                armed ? tone::alert.with_alpha(0.16f) : tone::raised);
        float x = r.x + 28;
        const float base = r.y + (ex.note.empty() ? 64 : 50);
        if (!wake.empty())
            x += p.text(wake, x, base, size::body, tone::muted) + 12;
        const float room = r.w - (x - r.x) - (runs ? 250 : 28);
        p.text(fill_room(ex.say, config_.room), x, base, size::body,
               runs ? tone::text : tone::muted, runs, Align::left, room);
        if (!ex.note.empty())
            p.text(ex.note, r.x + 28, base + 38, size::caption, tone::muted, false, Align::left,
                   r.w - 300);
        if (runs)
        {
            const Rect go{r.x + r.w - 220, r.y + (row_h - 72) / 2, 200, 72};
            if (asking)
                p.pill(go, "", "Asking\xE2\x80\xA6", PillKind::rest, false, false, true);
            else if (armed)
                p.pill(go, "", "Press again", PillKind::danger, false);
            else
                p.pill(go, "play", "Try it", PillKind::lead, false);
        }
        if (focused)
            p.focus(r, size::small_radius);
    }
    if (first_ex + rows_fit < examples)
        p.icon("chevron-down", panel.cx(), bottom + 6, 36, tone::muted);
    if (!cat.tip.empty())
    {
        p.icon("lightbulb-on-outline", panel.x + 52, panel.y + panel.h - 48, 40, tone::warm);
        p.text(cat.tip, panel.x + 90, panel.y + panel.h - 38, size::label, tone::muted, false,
               Align::left, panel.w - 130);
    }
}
} // namespace lamplight
