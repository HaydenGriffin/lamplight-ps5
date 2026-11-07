// Lamplight for PS5 - The payload's view of the console: the game in front, opening a title, rest.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "console.hpp"
#include "lamplight/json.hpp"
#include "platform/ps5/system.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#include <vector>

namespace
{
// The kernel's application record for a process. Packed: the payload SDK's ps sample declares
// it with a padded 64-bit field, which puts title_id four bytes late (seen on 7.20: "07923" for
// PPSA07923).
struct AppInfo
{
    std::uint32_t app_id;
    std::uint32_t unknown1[2];
    std::uint32_t app_type;
    char title_id[10];
    char unknown2[0x3c];
};
static_assert(offsetof(AppInfo, title_id) == 16);
struct LaunchParam
{
    std::uint32_t size;
    std::int32_t user;
    std::uint32_t options;
    std::uint64_t crash_report;
    std::uint32_t check_flag;
};
} // namespace

extern "C"
{
    int sceKernelGetAppInfo(pid_t pid, AppInfo *info);
    int sceLncUtilGetAppIdOfRunningBigApp();
    int sceSystemServiceGetAppIdOfRunningBigApp();
    int sceLncUtilKillApp(std::uint32_t app_id);
    int sceLncUtilSetAppFocus(std::uint32_t app_id, int flags);
    int sceLncUtilLaunchApp(const char *title_id, const char **argv, LaunchParam *param);
    int sceUserServiceInitialize(void *params);
    int sceUserServiceGetForegroundUser(std::int32_t *user);
    int sceKernelLoadStartModule(const char *path, std::size_t args, const void *argp,
                                 std::uint32_t flags, void *option, int *result);
    int sceKernelDlsym(int module, const char *name, void **address);
}

namespace lamplight::console
{
namespace
{
bool read_file(const std::string &path, std::vector<std::uint8_t> &out, std::size_t limit)
{
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file)
        return false;
    out.resize(limit);
    out.resize(std::fread(out.data(), 1, out.size(), file));
    std::fclose(file);
    return !out.empty();
}
// {"localizedParameters": {"defaultLanguage": "en-US", "en-US": {"titleName": ...}}}
std::string json_title(const std::vector<std::uint8_t> &data)
{
    const auto j = Json::parse(data.begin(), data.end(), nullptr, false);
    const auto &local = field(j, "localizedParameters");
    auto name = text(field(local, text(local, "defaultLanguage", "en-US")), "titleName");
    if (name.empty())
        name = text(field(local, "en-US"), "titleName");
    return name.empty() ? text(field(local, "en-GB"), "titleName") : name;
}
} // namespace

bool valid_title_id(const std::string &id)
{
    if (id.size() != 9)
        return false;
    for (std::size_t i = 0; i < 9; ++i)
        if (i < 4 ? !(id[i] >= 'A' && id[i] <= 'Z') : !(id[i] >= '0' && id[i] <= '9'))
            return false;
    return true;
}
std::string title_name(const std::string &title_id)
{
    if (!valid_title_id(title_id))
        return "";
    std::vector<std::uint8_t> data;
    for (const char *root : {"/user/appmeta/", "/system_data/priv/appmeta/"})
    {
        if (read_file(root + title_id + "/param.json", data, 512 * 1024))
            if (auto name = json_title(data); !name.empty())
                return name;
        if (read_file(root + title_id + "/param.sfo", data, 64 * 1024))
            if (auto name = sfo_title(data.data(), data.size()); !name.empty())
                return name;
    }
    return "";
}
Game running_game()
{
    // The system service's answer (as PS5Upload reads it), else the launcher's.
    int app = sceSystemServiceGetAppIdOfRunningBigApp();
    if (app <= 0 || app == -1)
        app = sceLncUtilGetAppIdOfRunningBigApp();
    if (app <= 0)
        return {};
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
    std::size_t size = 0;
    if (sysctl(mib, 4, nullptr, &size, nullptr, 0) != 0 || size == 0)
        return {};
    std::vector<std::uint8_t> buffer(size + 16 * sizeof(kinfo_proc));
    size = buffer.size();
    if (sysctl(mib, 4, buffer.data(), &size, nullptr, 0) != 0)
        return {};
    for (std::size_t at = 0; at + sizeof(kinfo_proc) <= size;)
    {
        const auto *process = reinterpret_cast<const kinfo_proc *>(buffer.data() + at);
        if (process->ki_structsize <= 0)
            break;
        at += static_cast<std::size_t>(process->ki_structsize);
        AppInfo info{};
        if (sceKernelGetAppInfo(process->ki_pid, &info) != 0 ||
            static_cast<int>(info.app_id) != app)
            continue;
        std::string id(info.title_id, strnlen(info.title_id, sizeof info.title_id));
        // A system app in front (the home screen's, settings) is not a game.
        if (!valid_title_id(id) || id.starts_with("NPXS"))
            return {};
        return {id, title_name(id), app};
    }
    return {};
}
Launch launch(const std::string &title_id)
{
    if (!valid_title_id(title_id))
        return Launch::failed;
    const auto front = running_game();
    if (front.title_id == title_id)
    {
        const int rc = sceLncUtilSetAppFocus(static_cast<std::uint32_t>(front.app_id), 0);
        hui::sys::log("[Lamplight notify] focus %s rc=%#x", title_id.c_str(), rc);
        return rc >= 0 ? Launch::opened : Launch::failed;
    }
    if (!front.title_id.empty())
    {
        if (front.title_id != "PPSA99484")
            return Launch::busy;
        const int killed = sceLncUtilKillApp(static_cast<std::uint32_t>(front.app_id));
        bool closed = false;
        for (int i = 0; i < 80 && killed >= 0 && !(closed = running_game().title_id.empty()); ++i)
            hui::sys::sleep_us(100000);
        if (!closed)
        {
            hui::sys::log("[Lamplight notify] Lamplight didn't close (%#x)", killed);
            return Launch::failed;
        }
    }
    sceUserServiceInitialize(nullptr);
    std::int32_t user = -1;
    if (sceUserServiceGetForegroundUser(&user) != 0)
        user = -1;
    LaunchParam param{sizeof(LaunchParam), user, 0, 0, 0};
    const int rc = sceLncUtilLaunchApp(title_id.c_str(), nullptr, &param);
    hui::sys::log("[Lamplight notify] launch %s rc=%#x", title_id.c_str(), rc);
    return rc >= 0 ? Launch::opened : Launch::failed;
}
// Looked up only when asked, in the libraries the call has lived in: a symbol this firmware lacks
// would stop a payload that imported it from starting at all.
bool rest()
{
    for (const char *path : {"/system/common/lib/libSceSystemService.sprx",
                             "/system/common/lib/libSceSystemStateMgr.sprx",
                             "/system/priv/lib/libSceSystemStateMgr.sprx"})
    {
        const int module = sceKernelLoadStartModule(path, 0, nullptr, 0, nullptr, nullptr);
        void *standby = nullptr;
        const int found =
            module < 0 ? module : sceKernelDlsym(module, "sceSystemStateMgrEnterStandby", &standby);
        hui::sys::log("[Lamplight notify] rest: %s module=%#x dlsym=%#x", path, module, found);
        if (found != 0 || !standby)
            continue;
        const int rc = reinterpret_cast<int (*)()>(standby)();
        hui::sys::log("[Lamplight notify] rest rc=%#x", rc);
        return rc >= 0;
    }
    return false;
}
} // namespace lamplight::console
