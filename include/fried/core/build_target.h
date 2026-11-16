#pragma once

#include "fried/core/platform.h"

#include <string>
#include <vector>

namespace fried::core
{
    // What a build compiles for. Windows from another system is a target of its own, while the game it produces is still the PC platform.
    enum class BuildTarget
    {
        Pc,
        WindowsFromHost,
        Vita,
        Switch,
        Nintendo3ds,
        NintendoDs,
        Cg50,
        Web
    };

    // Windows is not a target of its own on Windows, where it is the PC target.
    inline std::vector<BuildTarget> buildTargets(HostSystem host = hostSystem())
    {
        if (host == HostSystem::Windows)
        {
            return {BuildTarget::Pc,         BuildTarget::Vita, BuildTarget::Switch, BuildTarget::Nintendo3ds,
                    BuildTarget::NintendoDs, BuildTarget::Cg50, BuildTarget::Web};
        }
        return {BuildTarget::Pc,          BuildTarget::WindowsFromHost, BuildTarget::Vita, BuildTarget::Switch,
                BuildTarget::Nintendo3ds, BuildTarget::NintendoDs,      BuildTarget::Cg50, BuildTarget::Web};
    }

    inline Platform platformOf(BuildTarget target)
    {
        switch (target)
        {
        case BuildTarget::Vita:
            return Platform::Vita;
        case BuildTarget::Switch:
            return Platform::Switch;
        case BuildTarget::Nintendo3ds:
            return Platform::Nintendo3ds;
        case BuildTarget::NintendoDs:
            return Platform::NintendoDs;
        case BuildTarget::Cg50:
            return Platform::Cg50;
        case BuildTarget::Web:
            return Platform::Web;
        default:
            return Platform::Pc;
        }
    }

    inline std::string buildTargetKey(BuildTarget target)
    {
        return target == BuildTarget::WindowsFromHost ? "windows" : platformKey(platformOf(target));
    }

    // The PC target is named for the system it builds for.
    inline std::string buildTargetName(BuildTarget target, HostSystem host = hostSystem())
    {
        if (target == BuildTarget::WindowsFromHost)
        {
            return "Windows";
        }
        return target == BuildTarget::Pc ? hostSystemName(host) : platformLabel(platformOf(target));
    }
}
