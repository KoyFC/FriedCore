#pragma once

#include "fried/core/project.h"
#include "fried/core/yaml.h"

#include <string>

namespace fried::core
{
    inline constexpr const char *s_sceneSuffix = ".scene.yaml";
    inline constexpr const char *s_legacySceneSuffix = ".scene.json";

    // Whether a file name is a scene, written in YAML or still in JSON.
    bool isSceneFile(const std::string &fileName);

    // The name of the scene a file name holds, without the suffix, or an empty string for a file that is not a scene.
    std::string sceneName(const std::string &fileName);

    // The text of a scene as its file is written: JSON for a file that is still named .scene.json, which a project not yet moved to YAML keeps, and YAML for any other.
    std::string sceneText(const std::string &path, const Json &data);

    // The file of a scene of the project: the YAML one, or the JSON one when that is all there is, or the YAML one a new scene would be saved as.
    std::string scenePath(const Project &project, const std::string &name);
}
