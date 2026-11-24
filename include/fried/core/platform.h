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

    // The width of Haxe's Float in a build: single is a 32-bit float, which a platform with no floating point unit computes about a third faster, and double is a 64-bit double, which keeps whole numbers exact up to 2^53 and not 2^24.
    enum class Precision
    {
        Single,
        Double
    };

    // What a platform builds with when its section does not say. Only the DS, whose ARM9 has no floating point unit, defaults to single.
    inline Precision defaultPrecision(Platform platform)
    {
        return platform == Platform::NintendoDs ? Precision::Single : Precision::Double;
    }

    // Whether the engine builds the platform with either precision, which is every platform but the calculator, where single has not been tried.
    inline bool offersPrecision(Platform platform)
    {
        return platform != Platform::Cg50;
    }

    inline std::string precisionKey(Precision precision)
    {
        return precision == Precision::Single ? "single" : "double";
    }
}
