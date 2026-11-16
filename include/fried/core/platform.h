#pragma once

#include <array>
#include <string>

namespace fried::core
{
    enum class Platform
    {
        Pc,
        Vita,
        Switch,
        Nintendo3ds,
        NintendoDs,
        Cg50,
        Web
    };

    inline constexpr std::array<Platform, 7> s_platforms = {
        Platform::Pc, Platform::Vita, Platform::Switch, Platform::Nintendo3ds, Platform::NintendoDs, Platform::Cg50, Platform::Web};

    // The name the engine gives the platform, which is also its section of project.fried.
    inline std::string platformKey(Platform platform)
    {
        switch (platform)
        {
        case Platform::Vita:
            return "vita";
        case Platform::Switch:
            return "switch";
        case Platform::Nintendo3ds:
            return "3ds";
        case Platform::NintendoDs:
            return "nds";
        case Platform::Cg50:
            return "cg50";
        case Platform::Web:
            return "web";
        default:
            return "pc";
        }
    }

    inline std::string platformLabel(Platform platform)
    {
        switch (platform)
        {
        case Platform::Vita:
            return "PlayStation Vita";
        case Platform::Switch:
            return "Nintendo Switch";
        case Platform::Nintendo3ds:
            return "Nintendo 3DS";
        case Platform::NintendoDs:
            return "Nintendo DS";
        case Platform::Cg50:
            return "Casio fx-CG50";
        case Platform::Web:
            return "Web";
        default:
            return "PC";
        }
    }

    // The system a tool runs on, which is the one the PC target builds for.
    enum class HostSystem
    {
        Linux,
        Windows,
        MacOs
    };

    inline HostSystem hostSystem()
    {
#if defined(_WIN32)
        return HostSystem::Windows;
#elif defined(__APPLE__)
        return HostSystem::MacOs;
#else
        return HostSystem::Linux;
#endif
    }

    inline std::string hostSystemName(HostSystem host)
    {
        switch (host)
        {
        case HostSystem::Windows:
            return "Windows";
        case HostSystem::MacOs:
            return "macOS";
        default:
            return "Linux";
        }
    }
}
