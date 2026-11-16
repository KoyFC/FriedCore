#pragma once

#include <string>

namespace fried::core
{
    // The engine a project builds with: its engine/ submodule, or the folder above it, which is where the engine's own sandbox sits. Empty when neither holds an engine.
    std::string findEngine(const std::string &projectDirectory);
}
