#include "doctest/doctest.h"

#include "fried/core/android.h"

using namespace fried::core;

TEST_CASE("a project with no android.package is named after its organization and name")
{
    CHECK(android::defaultPackage("Fried Engine", "Fried Sandbox") == "com.friedengine.friedsandbox");
    CHECK(android::defaultPackage("", "3D Demo!") == "com.x3ddemo");
    CHECK(android::defaultPackage("", "") == "com.game");
}

TEST_CASE("a package Android refuses is told apart from one it takes")
{
    CHECK(android::isValidPackage("com.example.game"));
    CHECK_FALSE(android::isValidPackage("game"));
    CHECK_FALSE(android::isValidPackage("com.1example"));
    CHECK_FALSE(android::isValidPackage("com..game"));
}

TEST_CASE("a typed package is repaired into one Android takes")
{
    CHECK(android::sanitizedPackage("com.example.my game!") == "com.example.mygame");
    CHECK(android::sanitizedPackage("com..1game") == "com.x1game");
    CHECK(android::sanitizedPackage("mygame") == "mygame.game");
    CHECK(android::sanitizedPackage("") == "");
    CHECK(android::isValidPackage(android::sanitizedPackage("Com.Example._x")));
}
