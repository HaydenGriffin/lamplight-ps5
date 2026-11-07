// Lamplight for PS5 - Info: what the assistant just said, large.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/assistant.hpp"
#include "lamplight/paint.hpp"
#include <algorithm>

namespace lamplight
{
using hui::gfx::Align;
void Application::draw_info(Paint &p) const
{
    const bool kept = !config_.view_assist.empty() && !va_message_.empty() &&
                      answer_ == va_message_ &&
                      text(snapshot_.states.get(config_.view_assist).attributes, "mode") == "hold";
    std::string said;
    if (!answer_.empty())
    {
        said = answer_at_ ? "said at " + hhmm(answer_at_, clock_.zone) : "the last answer";
        said = answer_title_.empty() ? capitalised(said) : answer_title_ + " \xC2\xB7 " + said;
        if (kept)
            said += " \xC2\xB7 kept on screen";
    }
    draw_header(p, page_title(), said, kept ? tone::accent : tone::muted);
    const Rect area{size::margin, size::content_top, 1920 - 2 * size::margin,
                    size::content_bottom - size::content_top};
    if (answer_.empty())
    {
        p.empty(area, "message-text-outline", "Nothing to show",
                ready() ? "Ask something and the answer shows here. On What can I say?, press "
                          "Triangle to type a question."
                        : "Ask something once Home Assistant is back, and the answer shows here.");
        return;
    }
    if (is_misfire(answer_, config_.wake_phrase))
    {
        p.panel(area);
        const float cx = area.cx(), y = area.cy() - 150;
        p.disc(cx, y, size::disc_big, "microphone-question", DiscKind::raised2);
        p.text("Didn't catch that", cx, y + 130, size::headline, tone::text, true, Align::center);
        p.text(config_.wake_phrase.empty() ? "Try asking again."
                                           : "Say “" + config_.wake_phrase + "” and try again.",
               cx, y + 190, size::body, tone::muted, false, Align::center);
        p.text("It replied: " + answer_, cx, y + 250, size::label, tone::muted, false,
               Align::center, area.w - 120);
        return;
    }
    p.panel(area);
    Rect words = area.inset(48);
    // The picture beside the answer, when there is one and it loaded.
    if (const auto *picture = shown_picture(Pic::answer))
    {
        const int w = picture->width, h = picture->height;
        const float box = area.h - 96;
        const float scale = std::min(box / static_cast<float>(w), 720.0f / static_cast<float>(w));
        const float pw = static_cast<float>(w) * std::min(scale, box / static_cast<float>(h));
        const float ph = static_cast<float>(h) * std::min(scale, box / static_cast<float>(h));
        const Rect pic{area.x + 48, area.cy() - ph / 2, pw, ph};
        p.list.image(picture->texture, pic, hui::gfx::kFullUv, tone::white, size::small_radius);
        words = {pic.x + pic.w + 48, area.y + 48, area.x + area.w - 48 - (pic.x + pic.w + 48),
                 area.h - 96};
    }
    // The largest size, from 72 down to 40, at which the whole answer fits; below that it scrolls.
    const auto message = plain_reply(answer_);
    float sz = 72;
    hui::ui::ParagraphLines lines;
    for (;; sz -= 4)
    {
        lines = hui::ui::paragraph_lines(fonts_.text, message, sz, words.w);
        if (static_cast<float>(lines.lines.size()) * sz * 1.3f <= words.h || sz <= 40)
            break;
    }
    const float line_h = sz * 1.3f;
    const int fit = std::max(1, static_cast<int>(words.h / line_h));
    const int total = static_cast<int>(lines.lines.size());
    const int first = std::clamp(info_scroll_, 0, std::max(0, total - fit));
    float y = words.y + sz;
    for (int i = first; i < total && i < first + fit; ++i, y += line_h)
        p.text(lines.lines[static_cast<std::size_t>(i)], words.x, y, sz, tone::text);
    if (first + fit < total)
    {
        p.icon("chevron-down", words.cx() - 70, area.y + area.h - 30, 36, tone::muted);
        p.text("More below", words.cx() - 44, area.y + area.h - 20, size::caption, tone::muted);
    }
}
} // namespace lamplight
