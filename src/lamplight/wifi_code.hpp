// Lamplight for PS5 - The guest Wi-Fi QR code: WIFI:T:WPA;S:<ssid>;P:<password>;;
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <vector>

namespace lamplight
{
// The Wi-Fi joining text phones read from a QR code, with \ ; , : " escaped.
// security is WPA, WEP or nopass; an empty password means an open network.
std::string wifi_payload(const std::string &ssid, const std::string &password,
                         const std::string &security);
struct QrCode
{
    int size = 0; // modules a side; 0 when the text could not be encoded
    std::vector<bool> dark;
    bool at(int x, int y) const
    {
        return x >= 0 && y >= 0 && x < size && y < size &&
               dark[static_cast<std::size_t>(y * size + x)];
    }
};
QrCode encode_qr(const std::string &text);
} // namespace lamplight
