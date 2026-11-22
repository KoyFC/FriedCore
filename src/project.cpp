#include "fried/core/project.h"
#include "paths.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fried::core
{
    namespace
    {
        using Json = nlohmann::json;
        namespace fs = std::filesystem;

        const char *s_displayKey = "display";

        std::string stringOf(const Json &object, const std::string &key)
        {
            if (!object.is_object())
            {
                return std::string();
            }
            auto found = object.find(key);
            return found != object.end() && found->is_string() ? found->get<std::string>() : std::string();
        }

        std::string nested(const Json &root, const std::string &key, const std::string &subKey)
        {
            auto found = root.find(key);
            return found != root.end() ? stringOf(*found, subKey) : std::string();
        }

        void assign(Json &object, const std::string &key, const std::string &value)
        {
            if (value.empty())
            {
                object.erase(key);
            }
            else
            {
                object[key] = value;
            }
        }

        // A section left with no keys is removed with its last one.
        void assignSection(Json &root, const std::string &key, const Json &section)
        {
            if (section.empty())
            {
                root.erase(key);
            }
            else
            {
                root[key] = section;
            }
        }

        Json sectionOf(const Json &root, const std::string &key)
        {
            auto found = root.find(key);
            return found != root.end() && found->is_object() ? *found : Json::object();
        }

        void assignNested(Json &root, const std::string &key, const std::string &subKey, const std::string &value)
        {
            Json section = sectionOf(root, key);
            assign(section, subKey, value);
            assignSection(root, key, section);
        }

        Json displayObject(const DisplaySettings &display)
        {
            Json object = Json::object();
            object["mode"] = display.m_mode;
            if (display.m_mode != "default")
            {
                object["width"] = display.m_width;
                object["height"] = display.m_height;
            }
            object["filter"] = display.m_filter;
            return object;
        }

        bool isDigits(const std::string &value, std::size_t from, std::size_t count)
        {
            if (value.size() < from + count)
            {
                return false;
            }
            return std::all_of(value.begin() + from, value.begin() + from + count,
                               [](unsigned char character) { return std::isdigit(character) != 0; });
        }

        std::pair<int, int> nativeScreen(Platform platform)
        {
            switch (platform)
            {
            case Platform::Vita:
                return {960, 544};
            case Platform::Switch:
                return {1280, 720};
            case Platform::Nintendo3ds:
                return {400, 240};
            case Platform::NintendoDs:
                return {256, 192};
            case Platform::Cg50:
                return {396, 224};
            default:
                return {0, 0};
            }
        }
    }

    // The size of a native display is never written, so it cannot make two differ.
    bool DisplaySettings::operator==(const DisplaySettings &other) const
    {
        bool sameSize = m_mode == "default" || (m_width == other.m_width && m_height == other.m_height);
        return m_mode == other.m_mode && sameSize && m_filter == other.m_filter;
    }

    const std::vector<std::string> &displayModes()
    {
        static const std::vector<std::string> s_modes = {"default", "fit", "integer", "expand", "pixel", "height", "stretch"};
        return s_modes;
    }

    const std::vector<std::string> &displayFilters()
    {
        static const std::vector<std::string> s_filters = {"nearest", "linear"};
        return s_filters;
    }

    // Fields are checked by whoever edits them, not here.
    bool Project::exists(const std::string &directory)
    {
        std::error_code ignored;
        return fs::exists(pathOf(directory) / s_fileName, ignored);
    }

    bool Project::load(const std::string &directory, std::string *error)
    {
        if (!exists(directory))
        {
            *error = directory + " contains no " + s_fileName + ", so it is not a Fried project.";
            return false;
        }

        std::ifstream file(pathOf(directory) / s_fileName, std::ios::binary);
        if (!file)
        {
            *error = "Could not read " + directory + "/" + s_fileName + ".";
            return false;
        }
        std::stringstream text;
        text << file.rdbuf();
        return parse(directory, text.str(), error);
    }

    bool Project::parse(const std::string &directory, const std::string &text, std::string *error)
    {
        Json root;
        try
        {
            root = Json::parse(text);
        }
        catch (const Json::parse_error &problem)
        {
            *error = std::string(s_fileName) + " is not valid JSON: " + problem.what();
            return false;
        }

        if (!root.is_object())
        {
            *error = std::string(s_fileName) + " must hold a JSON object.";
            return false;
        }

        m_directory = directory;
        m_root = std::move(root);
        return true;
    }

    std::string Project::text() const
    {
        return m_root.dump(1, '\t') + "\n";
    }

    // Written beside the file and moved over it, so a failed write leaves the project as it was.
    bool Project::save(std::string *error) const
    {
        fs::path target = pathOf(filePath());
        fs::path staged = target;
        staged += ".tmp";

        {
            std::ofstream file(staged, std::ios::binary | std::ios::trunc);
            file << text();
            file.flush();
            if (!file)
            {
                *error = "Could not write " + textOf(target) + ".";
                std::error_code ignored;
                fs::remove(staged, ignored);
                return false;
            }
        }

        std::error_code failure;
        fs::rename(staged, target, failure);
        if (failure)
        {
            *error = "Could not write " + textOf(target) + ": " + failure.message();
            std::error_code ignored;
            fs::remove(staged, ignored);
            return false;
        }
        return true;
    }

    // Both become directory names through SDL_GetPrefPath.
    std::string Project::checkIdentity(const std::string &value)
    {
        if (value.empty())
        {
            return "must not be empty.";
        }
        if (value.find_first_of("\\/:*?\"<>|") != std::string::npos)
        {
            return R"(may not contain any of \ / : * ? " < > |, since it becomes a directory name.)";
        }
        return std::string();
    }

    std::string Project::checkVersion(const std::string &value)
    {
        bool hasShape = value.size() == 5 && isDigits(value, 0, 2) && value[2] == '.' && isDigits(value, 3, 2);
        if (!value.empty() && !hasShape)
        {
            return "must be two digits, a dot and two digits, which is what a Vita package requires (01.00).";
        }
        return std::string();
    }

    std::string Project::checkVitaTitleId(const std::string &value)
    {
        bool hasLetters = value.size() == 9 && std::all_of(value.begin(), value.begin() + 4, [](char character) {
            return character >= 'A' && character <= 'Z';
        });
        if (!value.empty() && !(hasLetters && isDigits(value, 4, 5)))
        {
            return "must be four capital letters followed by five digits (FRIE00001).";
        }
        return std::string();
    }

    std::string Project::name() const
    {
        return stringOf(m_root, "name");
    }

    std::string Project::organization() const
    {
        return stringOf(m_root, "organization");
    }

    std::string Project::version() const
    {
        return stringOf(m_root, "version");
    }

    std::string Project::windowTitle() const
    {
        return nested(m_root, "window", "title");
    }

    std::string Project::startScene() const
    {
        return stringOf(m_root, "startScene");
    }

    std::vector<std::string> Project::autoloads() const
    {
        std::vector<std::string> scenes;
        auto found = m_root.find("autoload");
        if (found != m_root.end() && found->is_array())
        {
            for (const Json &scene : *found)
            {
                if (scene.is_string())
                {
                    scenes.push_back(scene.get<std::string>());
                }
            }
        }
        return scenes;
    }

    std::string Project::vitaTitleId() const
    {
        return nested(m_root, "vita", "titleId");
    }

    std::string Project::packagingText(const std::string &section, const std::string &key) const
    {
        return nested(m_root, section, key);
    }

    // A key of the wrong type is read as missing, which is how the engine's build treats it.
    DisplaySettings Project::display() const
    {
        DisplaySettings display;
        Json object = sectionOf(m_root, s_displayKey);
        std::string mode = stringOf(object, "mode");
        std::string filter = stringOf(object, "filter");
        if (!mode.empty())
        {
            display.m_mode = mode;
        }
        if (!filter.empty())
        {
            display.m_filter = filter;
        }
        if (object.contains("width") && object["width"].is_number_integer())
        {
            display.m_width = object["width"].get<int>();
        }
        if (object.contains("height") && object["height"].is_number_integer())
        {
            display.m_height = object["height"].get<int>();
        }
        return display;
    }

    bool Project::vsync() const
    {
        auto found = m_root.find("vsync");
        return found == m_root.end() || !found->is_boolean() || found->get<bool>();
    }

    int Project::defaultTargetFramerate() const
    {
        auto found = m_root.find("defaultTargetFramerate");
        return found != m_root.end() && found->is_number_integer() ? std::max(0, found->get<int>()) : 0;
    }

    int Project::physicsRate() const
    {
        auto found = m_root.find("physicsRate");
        return found != m_root.end() && found->is_number_integer() && found->get<int>() > 0 ? found->get<int>() : s_defaultPhysicsRate;
    }

    void Project::setName(const std::string &value)
    {
        assign(m_root, "name", value);
    }

    void Project::setOrganization(const std::string &value)
    {
        assign(m_root, "organization", value);
    }

    void Project::setVersion(const std::string &value)
    {
        assign(m_root, "version", value);
    }

    void Project::setWindowTitle(const std::string &value)
    {
        assignNested(m_root, "window", "title", value);
    }

    void Project::setStartScene(const std::string &value)
    {
        assign(m_root, "startScene", value);
    }

    void Project::setAutoloads(const std::vector<std::string> &scenes)
    {
        if (scenes.empty())
        {
            m_root.erase("autoload");
        }
        else
        {
            m_root["autoload"] = scenes;
        }
    }

    void Project::setVitaTitleId(const std::string &value)
    {
        assignNested(m_root, "vita", "titleId", value);
    }

    void Project::setPackagingText(const std::string &section, const std::string &key, const std::string &value)
    {
        assignNested(m_root, section, key, value);
    }

    // Left untouched when unchanged, so a display written by hand keeps its shape.
    void Project::setDisplay(const DisplaySettings &value)
    {
        if (value != display())
        {
            m_root[s_displayKey] = displayObject(value);
        }
    }

    void Project::setVsync(bool value)
    {
        if (value)
        {
            m_root.erase("vsync");
        }
        else
        {
            m_root["vsync"] = false;
        }
    }

    void Project::setDefaultTargetFramerate(int value)
    {
        if (value > 0)
        {
            m_root["defaultTargetFramerate"] = value;
        }
        else
        {
            m_root.erase("defaultTargetFramerate");
        }
    }

    void Project::setPhysicsRate(int value)
    {
        if (value > 0 && value != s_defaultPhysicsRate)
        {
            m_root["physicsRate"] = value;
        }
        else
        {
            m_root.erase("physicsRate");
        }
    }

    std::vector<Platform> Project::declaredPlatforms() const
    {
        std::vector<Platform> declared;
        auto found = m_root.find("platforms");
        if (found == m_root.end() || !found->is_array())
        {
            return declared;
        }
        for (Platform platform : s_platforms)
        {
            for (const Json &entry : *found)
            {
                if (entry.is_string() && entry.get<std::string>() == platformKey(platform))
                {
                    declared.push_back(platform);
                    break;
                }
            }
        }
        return declared;
    }

    bool Project::declares(Platform platform) const
    {
        std::vector<Platform> declared = declaredPlatforms();
        return declared.empty() || std::find(declared.begin(), declared.end(), platform) != declared.end();
    }

    void Project::setDeclaredPlatforms(const std::vector<Platform> &platforms)
    {
        Json list = Json::array();
        for (Platform platform : s_platforms)
        {
            if (std::find(platforms.begin(), platforms.end(), platform) != platforms.end())
            {
                list.push_back(platformKey(platform));
            }
        }
        if (list.empty() || list.size() == s_platforms.size())
        {
            m_root.erase("platforms");
        }
        else
        {
            m_root["platforms"] = list;
        }
    }

    void Project::dropPlatformDisplays()
    {
        for (Platform platform : s_platforms)
        {
            std::string key = platformKey(platform);
            Json section = sectionOf(m_root, key);
            if (section.contains(s_displayKey))
            {
                section.erase(s_displayKey);
                assignSection(m_root, key, section);
            }
        }
    }

    std::pair<int, int> Project::logicalSize(Platform platform) const
    {
        DisplaySettings settings = display();
        std::pair<int, int> screen = nativeScreen(platform);
        bool hasDesign = settings.m_width > 0 && settings.m_height > 0;
        if (screen.first == 0)
        {
            return hasDesign ? std::make_pair(settings.m_width, settings.m_height) : std::make_pair(1280, 720);
        }
        if (settings.m_mode == "default" || !hasDesign)
        {
            return screen;
        }
        if (settings.m_mode == "expand")
        {
            double fit = std::min((double)screen.first / settings.m_width, (double)screen.second / settings.m_height);
            return {std::max(1, (int)std::lround(screen.first / fit)), std::max(1, (int)std::lround(screen.second / fit))};
        }
        if (settings.m_mode == "height")
        {
            return {std::max(1, (int)std::lround((double)screen.first * settings.m_height / screen.second)), settings.m_height};
        }
        if (settings.m_mode == "pixel")
        {
            double fit = std::min((double)screen.first / settings.m_width, (double)screen.second / settings.m_height);
            int scale = std::max(1, (int)std::floor(fit));
            return {std::max(1, screen.first / scale), std::max(1, screen.second / scale)};
        }
        return {settings.m_width, settings.m_height};
    }
}
