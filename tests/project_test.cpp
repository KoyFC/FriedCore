#include "doctest/doctest.h"

#include "fried/core/project.h"
#include "temporary_directory.h"

#include <vector>

using namespace fried::core;

namespace
{
    const std::string s_data = FRIED_CORE_TEST_DATA;

    Project parsed(const std::string &text)
    {
        Project project;
        std::string error;
        REQUIRE(project.parse("/games/sample", text, &error));
        return project;
    }
}

TEST_CASE("a project file gives its identity and one display for every platform")
{
    Project project = parsed(R"({
        "name": "Sample",
        "organization": "Org",
        "version": "01.00",
        "window": {"title": "Sample!"},
        "startScene": "main",
        "display": {"mode": "pixel", "width": 640, "height": 480, "filter": "linear"},
        "vita": {"titleId": "SAMP00001"},
        "3ds": {"description": "A sample"},
        "nds": {"display": {"mode": "default"}}
    })");

    CHECK(project.name() == "Sample");
    CHECK(project.organization() == "Org");
    CHECK(project.version() == "01.00");
    CHECK(project.windowTitle() == "Sample!");
    CHECK(project.startScene() == "main");
    CHECK(project.vitaTitleId() == "SAMP00001");
    CHECK(project.packagingText("3ds", "description") == "A sample");
    CHECK(project.packagingText("nds", "subtitle1").empty());
    CHECK(project.scenesDirectory() == "/games/sample/assets/scenes");
    CHECK(project.display() == DisplaySettings{"pixel", 640, 480, "linear"});
}

TEST_CASE("a project with no display uses the default mode, as the engine's build reads it")
{
    Project project = parsed(R"({"name": "Sample"})");
    CHECK(project.display().m_mode == "default");
    CHECK(project.display().m_filter == "nearest");
    CHECK(project.display().m_width == 0);
}

TEST_CASE("a file that is not a project is refused with a reason")
{
    Project project;
    std::string error;
    CHECK_FALSE(project.parse("/games/sample", "[1, 2]", &error));
    CHECK(error == "project.fried must hold a JSON object.");
    CHECK_FALSE(project.parse("/games/sample", "{\"name\": ", &error));
    CHECK(error.find("not valid JSON") != std::string::npos);
    CHECK_FALSE(project.load("/nonexistent-directory", &error));
    CHECK(error.find("not a Fried project") != std::string::npos);
}

TEST_CASE("a file the manager saved is written back byte for byte")
{
    std::string original = TemporaryDirectory::readFile(s_data + "/manager_saved.project.fried");
    CHECK(parsed(original).text() == original);
}

TEST_CASE("saving keeps the keys the model does not know")
{
    TemporaryDirectory directory("project-save");
    directory.write("project.fried", R"({"name": "Old", "gameClass": "Main", "switch": {"custom": 3}})");

    Project project;
    std::string error;
    REQUIRE(project.load(directory.path(), &error));
    project.setName("New");
    project.setVitaTitleId("FRIE00002");
    REQUIRE(project.save(&error));

    CHECK(directory.read("project.fried") ==
          "{\n\t\"gameClass\": \"Main\",\n\t\"name\": \"New\",\n\t\"switch\": {\n\t\t\"custom\": 3\n\t},\n\t\"vita\": {\n\t\t\"titleId\": \"FRIE00002\"\n\t}\n}\n");
    CHECK_FALSE(std::filesystem::exists(directory.path() + "/project.fried.tmp"));
}

TEST_CASE("the template's project file keeps everything through a save")
{
    Project project = parsed(TemporaryDirectory::readFile(s_data + "/template.project.fried"));
    Project again = parsed(project.text());
    CHECK(again.name() == "@NAME@");
    CHECK(again.windowTitle() == "@NAME@");
    CHECK(again.startScene() == "main");
    CHECK(again.vitaTitleId() == "@TITLE_ID@");
    CHECK(again.display() == DisplaySettings{"expand", 960, 540, "nearest"});
}

TEST_CASE("an emptied value removes its key, and its section with the last key")
{
    Project project = parsed(R"({"name": "Sample", "window": {"title": "T"}, "nds": {"subtitle1": "a", "subtitle2": "b"}})");
    project.setWindowTitle("");
    project.setPackagingText("nds", "subtitle1", "");
    project.setStartScene("");
    CHECK(project.text() == "{\n\t\"name\": \"Sample\",\n\t\"nds\": {\n\t\t\"subtitle2\": \"b\"\n\t}\n}\n");

    project.setPackagingText("nds", "subtitle2", "");
    CHECK(project.text() == "{\n\t\"name\": \"Sample\"\n}\n");
}

TEST_CASE("the display is written without a size in the default mode and left alone when unchanged")
{
    Project project = parsed(R"({"display": {"width": 640, "height": 480, "mode": "fit"}})");
    std::string before = project.text();
    project.setDisplay(DisplaySettings{"fit", 640, 480, "nearest"});
    CHECK(project.text() == before);

    project.setDisplay(DisplaySettings{"default", 640, 480, "linear"});
    CHECK(project.text() == "{\n\t\"display\": {\n\t\t\"filter\": \"linear\",\n\t\t\"mode\": \"default\"\n\t}\n}\n");
}

TEST_CASE("a platform's own display is dropped, and its section with it when nothing else is there")
{
    Project project = parsed(R"({"switch": {"display": {"filter": "nearest"}}, "vita": {"display": {"filter": "nearest"}, "titleId": "FRIE00001"}})");
    project.dropPlatformDisplays();
    CHECK(project.text() == "{\n\t\"vita\": {\n\t\t\"titleId\": \"FRIE00001\"\n\t}\n}\n");
}

TEST_CASE("identity, version and title id are checked the way packaging needs them")
{
    CHECK(Project::checkIdentity("Fried Sandbox").empty());
    CHECK_FALSE(Project::checkIdentity("").empty());
    CHECK_FALSE(Project::checkIdentity("a/b").empty());
    CHECK_FALSE(Project::checkIdentity("what?").empty());

    CHECK(Project::checkVersion("").empty());
    CHECK(Project::checkVersion("01.00").empty());
    CHECK_FALSE(Project::checkVersion("1.0").empty());
    CHECK_FALSE(Project::checkVersion("01.0a").empty());
    CHECK_FALSE(Project::checkVersion("01.000").empty());

    CHECK(Project::checkVitaTitleId("").empty());
    CHECK(Project::checkVitaTitleId("FRIE00001").empty());
    CHECK_FALSE(Project::checkVitaTitleId("frie00001").empty());
    CHECK_FALSE(Project::checkVitaTitleId("FRIE0001").empty());
    CHECK_FALSE(Project::checkVitaTitleId("FRIE0000A").empty());
}

TEST_CASE("the camera area follows the platform's screen and the display mode")
{
    auto sizes = [](const char *mode) {
        Project project = parsed(std::string(R"({"display": {"mode": ")") + mode + R"(", "width": 640, "height": 480}})");
        return std::vector<std::pair<int, int>>{project.logicalSize(Platform::Pc), project.logicalSize(Platform::NintendoDs),
                                                project.logicalSize(Platform::Nintendo3ds), project.logicalSize(Platform::Vita),
                                                project.logicalSize(Platform::Switch), project.logicalSize(Platform::Cg50)};
    };
    using Size = std::pair<int, int>;
    CHECK(sizes("default") == std::vector<Size>{{640, 480}, {256, 192}, {400, 240}, {960, 544}, {1280, 720}, {396, 224}});
    CHECK(sizes("fit") == std::vector<Size>(6, Size{640, 480}));
    CHECK(sizes("expand") == std::vector<Size>{{640, 480}, {640, 480}, {800, 480}, {847, 480}, {853, 480}, {849, 480}});
    CHECK(sizes("height") == std::vector<Size>{{640, 480}, {640, 480}, {800, 480}, {847, 480}, {853, 480}, {849, 480}});
    CHECK(sizes("pixel") == std::vector<Size>{{640, 480}, {256, 192}, {400, 240}, {960, 544}, {1280, 720}, {396, 224}});
}

TEST_CASE("a project in a folder with accents loads and saves")
{
    TemporaryDirectory folder("proyecto-ni\xC3\xB1o");
    std::string directory = folder.path();
    folder.write("project.fried", "{\"name\": \"Game\"}\n");

    fried::core::Project project;
    std::string error;
    REQUIRE(project.load(directory, &error));
    project.setVersion("01.00");
    REQUIRE(project.save(&error));

    fried::core::Project again;
    REQUIRE(again.load(directory, &error));
    CHECK(again.version() == "01.00");
}

TEST_CASE("vsync is on unless the project turns it off, and only an off is written")
{
    Project project = parsed(R"({"name": "Sample"})");
    CHECK(project.vsync());

    project.setVsync(false);
    CHECK_FALSE(project.vsync());
    CHECK(project.text() == "{\n\t\"name\": \"Sample\",\n\t\"vsync\": false\n}\n");

    project.setVsync(true);
    CHECK(project.vsync());
    CHECK(project.text() == "{\n\t\"name\": \"Sample\"\n}\n");

    CHECK(parsed(R"({"vsync": "no"})").vsync());
}

TEST_CASE("the physics rate is 60 unless the project gives another")
{
    Project project = parsed(R"({"name": "Sample"})");
    CHECK(project.physicsRate() == 60);

    project.setPhysicsRate(30);
    CHECK(project.physicsRate() == 30);
    CHECK(project.text() == "{\n\t\"name\": \"Sample\",\n\t\"physicsRate\": 30\n}\n");

    project.setPhysicsRate(60);
    CHECK(project.physicsRate() == 60);
    CHECK(project.text() == "{\n\t\"name\": \"Sample\"\n}\n");

    project.setPhysicsRate(30);
    project.setPhysicsRate(0);
    CHECK(project.text() == "{\n\t\"name\": \"Sample\"\n}\n");

    CHECK(parsed(R"({"physicsRate": -5})").physicsRate() == 60);
    CHECK(parsed(R"({"physicsRate": 0})").physicsRate() == 60);
    CHECK(parsed(R"({"physicsRate": "fast"})").physicsRate() == 60);
    CHECK(parsed(R"({"physicsRate": 120})").physicsRate() == 120);
}

TEST_CASE("the default target framerate is no limit unless the project gives one")
{
    Project project = parsed(R"({"name": "Sample"})");
    CHECK(project.defaultTargetFramerate() == 0);

    project.setDefaultTargetFramerate(144);
    CHECK(project.defaultTargetFramerate() == 144);
    CHECK(project.text() == "{\n\t\"defaultTargetFramerate\": 144,\n\t\"name\": \"Sample\"\n}\n");

    project.setDefaultTargetFramerate(0);
    CHECK(project.text() == "{\n\t\"name\": \"Sample\"\n}\n");

    CHECK(parsed(R"({"defaultTargetFramerate": -5})").defaultTargetFramerate() == 0);
    CHECK(parsed(R"({"defaultTargetFramerate": "fast"})").defaultTargetFramerate() == 0);
}

TEST_CASE("a project declares no platforms unless it says so, which is every one")
{
    Project project = parsed(R"({"name": "Sample"})");
    CHECK(project.declaredPlatforms().empty());
    for (Platform platform : fried::core::s_platforms)
    {
        CHECK(project.declares(platform));
    }

    project.setDeclaredPlatforms({Platform::NintendoDs, Platform::Pc});
    CHECK(project.declaredPlatforms() == std::vector<Platform>{Platform::Pc, Platform::NintendoDs});
    CHECK(project.declares(Platform::Pc));
    CHECK(project.declares(Platform::NintendoDs));
    CHECK_FALSE(project.declares(Platform::Vita));
    CHECK(project.text() == "{\n\t\"name\": \"Sample\",\n\t\"platforms\": [\n\t\t\"pc\",\n\t\t\"nds\"\n\t]\n}\n");

    project.setDeclaredPlatforms({});
    CHECK(project.text() == "{\n\t\"name\": \"Sample\"\n}\n");

    project.setDeclaredPlatforms({Platform::Cg50});
    project.setDeclaredPlatforms(std::vector<Platform>(fried::core::s_platforms.begin(), fried::core::s_platforms.end()));
    CHECK(project.text() == "{\n\t\"name\": \"Sample\"\n}\n");

    CHECK(parsed(R"({"platforms": ["web", "gamecube"]})").declaredPlatforms() == std::vector<Platform>{Platform::Web});
    CHECK(parsed(R"({"platforms": "pc"})").declaredPlatforms().empty());
}

TEST_CASE("the autoloads are read in order and an empty list removes the key")
{
    Project project = parsed(R"({"name": "Sample", "autoload": ["shell", "music", 3]})");
    CHECK(project.autoloads() == std::vector<std::string>({"shell", "music"}));

    project.setAutoloads({"music"});
    CHECK(project.text() == "{\n\t\"autoload\": [\n\t\t\"music\"\n\t],\n\t\"name\": \"Sample\"\n}\n");

    project.setAutoloads({});
    CHECK(project.text() == "{\n\t\"name\": \"Sample\"\n}\n");
}

TEST_CASE("the input block is kept whole, and an empty one removes the key")
{
    Project project = parsed(R"({"name": "Sample", "input": {"maps": [{"name": "Gameplay"}]}})");
    CHECK(project.inputBlock()["maps"][0]["name"] == "Gameplay");

    project.setInputBlock({{"maps", nlohmann::json::array({{{"name", "Menu"}}})}});
    CHECK(project.inputBlock()["maps"][0]["name"] == "Menu");

    project.setInputBlock({{"maps", nlohmann::json::array()}});
    CHECK(project.inputBlock().empty());
    CHECK(project.text() == "{\n\t\"name\": \"Sample\"\n}\n");
}
