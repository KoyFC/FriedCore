#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace fried::core
{
    using Json = nlohmann::json;

    // The Fried YAML subset the engine reads (docs/reference/yaml.md in the engine), which is a superset of JSON, so a file still in JSON parses. The error is "line:column: message".
    bool parseYaml(const std::string &text, Json *value, std::string *error);

    // Plain orders keys alphabetically. Scene puts the keys of a scene file in the order a person reads them: version, name, camera and objects at the top, then id, name, instance, active, transform and components of an object first and its children last, and type first in a transform or a component.
    enum class YamlLayout
    {
        Plain,
        Scene
    };

    std::string emitYaml(const Json &value, YamlLayout layout);
}
