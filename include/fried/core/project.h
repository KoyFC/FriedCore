#pragma once

#include "fried/core/platform.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

namespace fried::core
{
    // The size is only meaningful when the mode is not "default", where the game takes its window's or its screen's size.
    struct DisplaySettings
    {
        std::string m_mode = "default";
        int m_width = 0;
        int m_height = 0;
        std::string m_filter = "nearest";

        bool operator==(const DisplaySettings &other) const;
        bool operator!=(const DisplaySettings &other) const { return !(*this == other); }
    };

    // What a tool proposes when a project leaves the "default" mode and has no size yet.
    inline const DisplaySettings s_suggestedDisplay = {"expand", 640, 360, "nearest"};

    // The modes and filters the engine accepts, in the order a tool lists them.
    const std::vector<std::string> &displayModes();
    const std::vector<std::string> &displayFilters();

    // A project's project.fried, read and written the way the engine's build reads it.
    class Project
    {
    public:
        static constexpr const char *s_fileName = "project.fried";

        // The longest text the engine's packaging accepts for each line.
        static constexpr int s_3dsDescriptionLimit = 128;
        static constexpr int s_ndsSubtitleLimit = 31;

        // True when the directory holds a project file, whether or not it parses.
        static bool exists(const std::string &directory);

        // Each returns false and says why in error.
        bool load(const std::string &directory, std::string *error);
        bool parse(const std::string &directory, const std::string &text, std::string *error);
        bool save(std::string *error) const;

        // The file as save writes it.
        std::string text() const;

        // Each returns what is wrong with the value, or an empty string.
        static std::string checkIdentity(const std::string &value);
        static std::string checkVersion(const std::string &value);
        static std::string checkVitaTitleId(const std::string &value);

        const std::string &directory() const { return m_directory; }
        std::string filePath() const { return m_directory + "/" + s_fileName; }
        std::string assetsDirectory() const { return m_directory + "/assets"; }
        std::string scenesDirectory() const { return m_directory + "/assets/scenes"; }

        std::string name() const;
        std::string organization() const;
        std::string version() const;
        std::string windowTitle() const;
        // The scene the game opens with, by its file's name without ".scene.json".
        std::string startScene() const;
        std::string vitaTitleId() const;
        // A line of packaging text a platform's section may give, empty when it falls back to the game's identity.
        std::string packagingText(const std::string &section, const std::string &key) const;
        // The top-level "display", which every platform shares.
        DisplaySettings display() const;
        // Whether a desktop window waits for the display's refresh, true when the project does not say. Consoles and the calculator ignore it.
        bool vsync() const;
        // The frames a second a game starts with when vsync is off, where 0 is no limit. A game may change it while it runs, so this is only where it begins.
        int defaultTargetFramerate() const;
        // The fixed steps a second a game runs its physics at. The engine reads 60 when the project does not say, and so does this.
        int physicsRate() const;
        static constexpr int s_defaultPhysicsRate = 60;

        // An empty value removes the key, which the engine reads as undeclared.
        void setName(const std::string &value);
        void setOrganization(const std::string &value);
        void setVersion(const std::string &value);
        void setWindowTitle(const std::string &value);
        void setStartScene(const std::string &value);
        void setVitaTitleId(const std::string &value);
        void setPackagingText(const std::string &section, const std::string &key, const std::string &value);
        void setDisplay(const DisplaySettings &value);
        // Only a vsync turned off is written, so the default leaves no key behind.
        void setVsync(bool value);
        // Only a limit above zero is written, since no limit is what the key's absence means.
        void setDefaultTargetFramerate(int value);
        // Only a rate other than the default is written, so the default leaves no key behind.
        void setPhysicsRate(int value);
        // A display of a platform's own is no longer read by the engine, which refuses to build with one.
        void dropPlatformDisplays();

        // The size the game draws in on the platform's own screen, which is what the camera shows: the screen itself in `default`, the design size in `fit`, `integer` and `stretch`, and the design size widened to cover the screen in `expand`.
        // A PC or web window has no fixed screen, so it opens at the design size.
        std::pair<int, int> logicalSize(Platform platform) const;

    private:
        std::string m_directory;

        // Kept whole so unknown keys survive a save.
        nlohmann::json m_root = nlohmann::json::object();
    };
}
