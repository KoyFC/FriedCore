#include "doctest/doctest.h"

#include "fried/core/yaml.h"
#include "temporary_directory.h"

#include <algorithm>
#include <filesystem>
#include <vector>

using namespace fried::core;
namespace fs = std::filesystem;

namespace
{
    const std::string s_data = FRIED_CORE_TEST_DATA;

    std::string trimmed(std::string text)
    {
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' '))
        {
            text.pop_back();
        }
        return text;
    }

    std::vector<fs::path> corpus()
    {
        std::vector<fs::path> files;
        for (const fs::directory_entry &entry : fs::directory_iterator(s_data + "/yaml-corpus"))
        {
            if (entry.path().extension() == ".yaml")
            {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    Json parsedFile(const std::string &path)
    {
        Json value;
        std::string error;
        INFO(path << ": " << error);
        REQUIRE(parseYaml(TemporaryDirectory::readFile(path), &value, &error));
        return value;
    }
}

TEST_CASE("every case of the corpus gives the JSON it is written next to, or the error")
{
    REQUIRE(!corpus().empty());
    for (const fs::path &file : corpus())
    {
        std::string name = file.stem().string();
        CAPTURE(name);
        Json value;
        std::string error;
        bool parsed = parseYaml(TemporaryDirectory::readFile(file.string()), &value, &error);
        if (name.rfind("error-", 0) == 0)
        {
            fs::path expected = file;
            expected.replace_extension(".txt");
            CHECK_FALSE(parsed);
            CHECK(error == trimmed(TemporaryDirectory::readFile(expected.string())));
        }
        else
        {
            fs::path expected = file;
            expected.replace_extension(".json");
            CHECK(parsed);
            CHECK(value == Json::parse(TemporaryDirectory::readFile(expected.string())));
        }
    }
}

TEST_CASE("what the emitter writes reads back as the same data")
{
    for (const fs::path &file : corpus())
    {
        if (file.stem().string().rfind("error-", 0) == 0)
        {
            continue;
        }
        fs::path expected = file;
        expected.replace_extension(".json");
        Json data = Json::parse(TemporaryDirectory::readFile(expected.string()));
        for (YamlLayout layout : {YamlLayout::Plain, YamlLayout::Scene})
        {
            std::string text = emitYaml(data, layout);
            Json back;
            std::string error;
            INFO(file.stem().string() << ": " << error << "\n" << text);
            REQUIRE(parseYaml(text, &back, &error));
            CHECK(back == data);
            CHECK(back.dump() == data.dump());
        }
    }
}

TEST_CASE("the scenes and the project of the engine's sandbox survive a round trip")
{
    for (const fs::directory_entry &entry : fs::directory_iterator(s_data + "/engine"))
    {
        Json data = parsedFile(entry.path().string());
        bool isScene = entry.path().string().find(".scene.json") != std::string::npos;
        std::string text = emitYaml(data, isScene ? YamlLayout::Scene : YamlLayout::Plain);
        Json back;
        std::string error;
        INFO(entry.path().string() << ": " << error << "\n" << text);
        REQUIRE(parseYaml(text, &back, &error));
        CHECK(back.dump() == data.dump());
    }
}

TEST_CASE("a scene is written with its keys in reading order and flat values in one line")
{
    Json scene = Json::parse(R"({"nodes":[{"children":[{"id":"b","type":"Camera","x":1,"name":"B"}],"z":2,"type":"Node2D","id":"a","color":{"g":2,"r":1}}],"name":"S","version":2,"camera":"b"})");
    CHECK(emitYaml(scene, YamlLayout::Scene) ==
          "version: 2\n"
          "name: S\n"
          "camera: b\n"
          "nodes:\n"
          "  - type: Node2D\n"
          "    id: a\n"
          "    color: {r: 1, g: 2}\n"
          "    z: 2\n"
          "    children:\n"
          "      - type: Camera\n"
          "        id: b\n"
          "        name: B\n"
          "        x: 1\n");
}

TEST_CASE("text that would read as another kind of value is quoted")
{
    Json data = Json::object();
    data["version"] = "01.00";
    data["word"] = "true";
    data["empty"] = "";
    data["colon"] = "a: b";
    data["plain"] = "hello world";
    data["float"] = 2.0;
    std::string text = emitYaml(data, YamlLayout::Plain);
    CHECK(text.find("version: \"01.00\"") != std::string::npos);
    CHECK(text.find("word: \"true\"") != std::string::npos);
    CHECK(text.find("empty: \"\"") != std::string::npos);
    CHECK(text.find("colon: \"a: b\"") != std::string::npos);
    CHECK(text.find("plain: hello world") != std::string::npos);
    CHECK(text.find("float: 2.0") != std::string::npos);
}
