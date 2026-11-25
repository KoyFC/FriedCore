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

TEST_CASE("the devices adb lists are told ready from waiting")
{
    std::vector<android::Device> devices = android::parseDevices(
        "* daemon not running; starting now at tcp:5037\r\n* daemon started successfully\r\nList of devices attached\r\n"
        "R58M123ABC\tdevice\r\nemulator-5554\toffline\r\n0123456789\tunauthorized usb:1-2\r\n\r\n");
    REQUIRE(devices.size() == 3);
    CHECK(devices[0].m_serial == "R58M123ABC");
    CHECK(devices[0].isReady());
    CHECK(devices[1].m_state == "offline");
    CHECK_FALSE(devices[1].isReady());
    CHECK(devices[2].m_state == "unauthorized");

    CHECK(android::parseDevices("List of devices attached\n\n").empty());
}

TEST_CASE("a project is installed as the package it declares, or the one made for it")
{
    CHECK(android::packageOf("com.example.game", "Org", "Name") == "com.example.game");
    CHECK(android::packageOf("", "Fried Engine", "Fried Sandbox") == "com.friedengine.friedsandbox");
}
