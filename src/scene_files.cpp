#include "fried/core/scene_files.h"
#include "paths.h"

#include <filesystem>

namespace fried::core
{
    namespace
    {
        bool endsWith(const std::string &text, const std::string &end)
        {
            return text.size() >= end.size() && text.compare(text.size() - end.size(), end.size(), end) == 0;
        }
    }

    bool isSceneFile(const std::string &fileName)
    {
        return !sceneName(fileName).empty();
    }

    std::string sceneName(const std::string &fileName)
    {
        for (const std::string suffix : {std::string(s_sceneSuffix), std::string(s_legacySceneSuffix)})
        {
            if (fileName.size() > suffix.size() && endsWith(fileName, suffix))
            {
                return fileName.substr(0, fileName.size() - suffix.size());
            }
        }
        return std::string();
    }

    std::string sceneText(const std::string &path, const Json &data)
    {
        if (endsWith(path, s_legacySceneSuffix))
        {
            return data.dump(1, '\t') + "\n";
        }
        return emitYaml(data, YamlLayout::Scene);
    }

    std::string scenePath(const Project &project, const std::string &name)
    {
        std::string yaml = project.scenesDirectory() + "/" + name + s_sceneSuffix;
        std::string json = project.scenesDirectory() + "/" + name + s_legacySceneSuffix;
        std::error_code ignored;
        if (!std::filesystem::exists(pathOf(yaml), ignored) && std::filesystem::exists(pathOf(json), ignored))
        {
            return json;
        }
        return yaml;
    }
}
