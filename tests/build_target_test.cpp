#include "doctest/doctest.h"

#include "fried/core/build_target.h"

#include <algorithm>

using namespace fried::core;

TEST_CASE("every platform has the key the engine's build gives it")
{
    std::vector<std::string> keys;
    for (Platform platform : s_platforms)
    {
        keys.push_back(platformKey(platform));
    }
    CHECK(keys == std::vector<std::string>{"pc", "vita", "switch", "3ds", "nds", "cg50", "web"});
}

TEST_CASE("Windows is a target of its own only away from Windows")
{
    auto offers = [](HostSystem host) {
        std::vector<BuildTarget> targets = buildTargets(host);
        return std::find(targets.begin(), targets.end(), BuildTarget::WindowsFromHost) != targets.end();
    };
    CHECK(offers(HostSystem::Linux));
    CHECK(offers(HostSystem::MacOs));
    CHECK_FALSE(offers(HostSystem::Windows));
}

TEST_CASE("a build target names the platform its game runs on")
{
    CHECK(platformOf(BuildTarget::WindowsFromHost) == Platform::Pc);
    CHECK(platformOf(BuildTarget::Web) == Platform::Web);
    CHECK(buildTargetKey(BuildTarget::WindowsFromHost) == "windows");
    CHECK(buildTargetKey(BuildTarget::Web) == "web");
    CHECK(buildTargetKey(BuildTarget::NintendoDs) == "nds");

    CHECK(buildTargetName(BuildTarget::Pc, HostSystem::Linux) == "Linux");
    CHECK(buildTargetName(BuildTarget::Pc, HostSystem::Windows) == "Windows");
    CHECK(buildTargetName(BuildTarget::WindowsFromHost, HostSystem::Linux) == "Windows");
    CHECK(buildTargetName(BuildTarget::Cg50, HostSystem::Linux) == "Casio fx-CG50");
}
