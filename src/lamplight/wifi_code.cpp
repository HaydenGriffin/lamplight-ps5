// Lamplight for PS5 - The guest Wi-Fi QR code: WIFI:T:WPA;S:<ssid>;P:<password>;;
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/wifi_code.hpp"
#include <cstdint>
extern "C"
{
#include "../../third_party/qrcodegen/qrcodegen.h"
}

namespace lamplight
{
namespace
{
std::string escaped(const std::string &value)
{
    std::string out;
    for (char c : value)
    {
        if (c == '\\' || c == ';' || c == ',' || c == ':' || c == '"')
            out += '\\';
        out += c;
    }
    return out;
}
} // namespace
std::string wifi_payload(const std::string &ssid, const std::string &password,
                         const std::string &security)
{
    const std::string type = password.empty() ? "nopass" : security.empty() ? "WPA" : security;
    std::string out = "WIFI:T:" + type + ";S:" + escaped(ssid) + ";";
    if (!password.empty())
        out += "P:" + escaped(password) + ";";
    return out + ";";
}
QrCode encode_qr(const std::string &text)
{
    QrCode out;
    std::vector<std::uint8_t> code(qrcodegen_BUFFER_LEN_MAX), scratch(qrcodegen_BUFFER_LEN_MAX);
    // Medium error correction, as phone cameras read it well at a distance from a TV.
    if (text.empty() || text.size() > 1000 ||
        !qrcodegen_encodeText(text.c_str(), scratch.data(), code.data(), qrcodegen_Ecc_MEDIUM,
                              qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO,
                              true))
        return out;
    out.size = qrcodegen_getSize(code.data());
    out.dark.resize(static_cast<std::size_t>(out.size * out.size));
    for (int y = 0; y < out.size; ++y)
        for (int x = 0; x < out.size; ++x)
            out.dark[static_cast<std::size_t>(y * out.size + x)] =
                qrcodegen_getModule(code.data(), x, y);
    return out;
}
} // namespace lamplight
