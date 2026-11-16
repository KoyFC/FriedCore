#include "doctest/doctest.h"

#include "fried/core/engine_location.h"
#include "temporary_directory.h"

#include <filesystem>

using namespace fried::core;

TEST_CASE("the engine is the project's submodule, or the folder above the project")
{
    TemporaryDirectory root("engine-location");
    root.write("game/engine/cmake/FriedEngine.cmake", "");
    root.write("engine/cmake/FriedEngine.cmake", "");
    root.write("engine/sandbox/project.fried", "{}");
    root.write("alone/project.fried", "{}");

    std::string base = std::filesystem::weakly_canonical(root.path()).string();
    CHECK(findEngine(root.path() + "/game") == base + "/game/engine");
    CHECK(findEngine(root.path() + "/engine/sandbox") == base + "/engine");
    CHECK(findEngine(root.path() + "/alone").empty());
}
