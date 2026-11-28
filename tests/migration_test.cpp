#include "doctest/doctest.h"

#include "fried/core/migration.h"
#include "fried/core/scene_files.h"
#include "fried/core/yaml.h"
#include "temporary_directory.h"

#include <filesystem>

using namespace fried::core;
namespace fs = std::filesystem;

namespace
{
    const std::string s_data = FRIED_CORE_TEST_DATA;

    Json parsedText(const std::string &text)
    {
        Json value;
        std::string error;
        INFO(error << "\n" << text);
        REQUIRE(parseYaml(text, &value, &error));
        return value;
    }

    void writeJsonProject(const TemporaryDirectory &directory)
    {
        directory.write("project.fried", TemporaryDirectory::readFile(s_data + "/engine/sandbox.project.fried"));
        directory.write("assets/scenes/sandbox_3d.scene.json", TemporaryDirectory::readFile(s_data + "/engine/sandbox_3d.scene.json"));
        directory.write("assets/sprite.png.import", "{\n\t\"image\": {\"maxSize\": 256}\n}\n");
        directory.write("assets/readme.txt", "{ not a sidecar }");
    }
}

TEST_CASE("scene file names are told from other files in either form")
{
    CHECK(isSceneFile("main.scene.yaml"));
    CHECK(isSceneFile("main.scene.json"));
    CHECK_FALSE(isSceneFile("main.json"));
    CHECK_FALSE(isSceneFile(".scene.yaml"));
    CHECK(sceneName("level 2.scene.json") == "level 2");
    CHECK(sceneName("level.scene.yaml") == "level");
}

TEST_CASE("the path of a scene is the YAML file, or the JSON one when that is all there is")
{
    TemporaryDirectory directory("scene-path");
    directory.write("project.fried", "name: Sample\n");
    directory.write("assets/scenes/old.scene.json", "{}");
    directory.write("assets/scenes/both.scene.json", "{}");
    directory.write("assets/scenes/both.scene.yaml", "{}");
    Project project;
    std::string error;
    REQUIRE(project.load(directory.path(), &error));
    CHECK(scenePath(project, "old") == project.scenesDirectory() + "/old.scene.json");
    CHECK(scenePath(project, "both") == project.scenesDirectory() + "/both.scene.yaml");
    CHECK(scenePath(project, "new") == project.scenesDirectory() + "/new.scene.yaml");
}

TEST_CASE("a project in JSON is rewritten in YAML with the same data and a backup")
{
    TemporaryDirectory directory("migrate");
    writeJsonProject(directory);
    Json project = parsedText(directory.read("project.fried"));
    Json scene = parsedText(directory.read("assets/scenes/sandbox_3d.scene.json"));

    REQUIRE(needsYamlMigration(directory.path()));
    MigrationReport report;
    std::string error;
    REQUIRE(migrateToYaml(directory.path(), &report, &error));

    CHECK(report.m_failed.empty());
    CHECK(report.m_converted.size() == 3);
    CHECK_FALSE(needsYamlMigration(directory.path()));
    CHECK(parsedText(directory.read("project.fried")) == project);
    CHECK(directory.read("project.fried").front() != '{');
    CHECK(parsedText(directory.read("assets/scenes/sandbox_3d.scene.yaml")) == scene);
    CHECK_FALSE(fs::exists(fs::path(directory.path()) / "assets/scenes/sandbox_3d.scene.json"));
    CHECK(directory.read("assets/sprite.png.import").front() != '{');
    CHECK(directory.read("assets/readme.txt") == "{ not a sidecar }");

    fs::path backup = fs::path(directory.path()) / report.m_backupDirectory;
    CHECK(TemporaryDirectory::readFile((backup / "assets/scenes/sandbox_3d.scene.json").string()) == TemporaryDirectory::readFile(s_data + "/engine/sandbox_3d.scene.json"));
    CHECK(fs::exists(backup / "project.fried"));
}

TEST_CASE("a project already in YAML needs nothing")
{
    TemporaryDirectory directory("migrate-none");
    directory.write("project.fried", "name: Sample\n");
    directory.write("assets/scenes/main.scene.yaml", "version: 2\n");
    CHECK_FALSE(needsYamlMigration(directory.path()));
    MigrationReport report;
    std::string error;
    CHECK(migrateToYaml(directory.path(), &report, &error));
    CHECK(report.m_converted.empty());
    CHECK_FALSE(fs::exists(fs::path(directory.path()) / ".fried"));
}

TEST_CASE("a scene that already has a YAML file and a file that is not valid are left alone and reported")
{
    TemporaryDirectory directory("migrate-fail");
    directory.write("project.fried", "{\"name\": \"Sample\"}");
    directory.write("assets/scenes/a.scene.json", "{\"version\": 2}");
    directory.write("assets/scenes/a.scene.yaml", "version: 2\n");
    directory.write("assets/scenes/b.scene.json", "{\"version\": ");
    MigrationReport report;
    std::string error;
    REQUIRE(migrateToYaml(directory.path(), &report, &error));
    CHECK(report.m_converted.size() == 1);
    CHECK(report.m_failed.size() == 2);
    CHECK(fs::exists(fs::path(directory.path()) / "assets/scenes/a.scene.json"));
    CHECK(fs::exists(fs::path(directory.path()) / "assets/scenes/b.scene.json"));
}

TEST_CASE("a scene keeps the format of the file it is saved to")
{
    Json data = Json::parse(R"({"version": 3, "name": "S", "objects": []})");
    CHECK(sceneText("a/main.scene.json", data).front() == '{');
    CHECK(sceneText("a/main.scene.yaml", data) == "version: 3\nname: S\nobjects: []\n");
}
