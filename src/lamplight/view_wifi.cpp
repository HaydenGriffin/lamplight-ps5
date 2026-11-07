// Lamplight for PS5 - Wi-Fi: a QR code big enough to scan from the sofa.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"

namespace lamplight
{
using hui::gfx::Align;
void Application::draw_wifi(Paint &p) const
{
    draw_header(p, "Wi-Fi", "For guests and new devices", tone::muted);
    if (config_.wifi_ssid.empty() || !qr_.size)
    {
        p.empty({size::margin, size::content_top, 1728, 700}, "wifi", "No guest network yet",
                "Add the network's name and password in Setup (Options). The password is only "
                "ever shown inside the code.");
        return;
    }
    // A network to join isn't "on", so nothing here is warm (The Lamp Rule). The code sits on
    // white with at least four modules of quiet space, as phone cameras need.
    const float side = size::content_bottom - size::content_top;
    const Rect code{size::margin, size::content_top, side, side};
    p.list.shadow(code.inset(-6), 40, 28, Color::rgb(0x000000, 0.45f));
    p.list.rounded_rect(code, 40, tone::white);
    const float module =
        static_cast<float>(static_cast<int>((side - 40) / static_cast<float>(qr_.size + 8)));
    const float origin_x = code.x + (side - module * static_cast<float>(qr_.size)) / 2;
    const float origin_y = code.y + (side - module * static_cast<float>(qr_.size)) / 2;
    // Runs of dark modules along a row are one rectangle each.
    for (int y = 0; y < qr_.size; ++y)
        for (int x = 0; x < qr_.size;)
        {
            if (!qr_.at(x, y))
            {
                ++x;
                continue;
            }
            int end = x;
            while (end < qr_.size && qr_.at(end, y))
                ++end;
            p.list.rounded_rect({origin_x + static_cast<float>(x) * module,
                                 origin_y + static_cast<float>(y) * module,
                                 static_cast<float>(end - x) * module, module},
                                0, Color::rgb(0x000000));
            x = end;
        }
    const Rect info{code.x + side + 40, size::content_top,
                    1920 - size::margin - (code.x + side + 40),
                    size::content_bottom - size::content_top};
    p.panel(info);
    float y = info.y + 70;
    p.disc(info.x + 76, y + 20, size::disc + 8, "wifi", DiscKind::raised2);
    y += 130;
    p.text("Network", info.x + 40, y, size::label, tone::muted);
    y = p.paragraph(config_.wifi_ssid, info.x + 40, y + 70, size::headline, info.w - 80, 64,
                    tone::text, 2, true);
    y += 40;
    const char *steps[] = {"Open your phone's camera", "Point it at the code", "Tap "};
    for (int i = 0; i < 3; ++i, y += 92)
    {
        p.list.circle(info.x + 64, y - 14, 30, tone::raised2);
        p.number(std::to_string(i + 1), info.x + 64, y, size::button, tone::text, Align::center);
        const float w = p.text(steps[i], info.x + 116, y, size::body + 2, tone::text);
        if (i == 2)
            p.text("Join", info.x + 116 + w, y, size::body + 2, tone::text, true);
    }
}
} // namespace lamplight
