#pragma once

#include "fried/core/build_target.h"
#include "fried/core/toolchains.h"

#include <map>
#include <string>
#include <vector>

namespace fried::core
{
    enum class BuildType
    {
        Debug,
        Release
    };

    std::string buildTypeName(BuildType type);
    BuildType buildTypeNamed(const std::string &name);

    // One program a build runs, in the project's folder.
    struct BuildStep
    {
        std::vector<std::string> m_arguments;

        // Set on top of the environment the tool itself runs in.
        std::map<std::string, std::string> m_environment;
    };

    class Build
    {
    public:
        // Relative to the project's folder.
        static std::string defaultDirectory(BuildTarget target);

        // Debug builds the Haxe side with -debug, which is what makes the engine log where each message came from.
        static BuildStep haxeStep(BuildType type, bool logToFile);

        // On Windows without Visual Studio, until a tool has downloaded a compiler of its own.
        static bool needsCompilerDownload(BuildTarget target, const Toolchains &toolchains);
        // The web build, with no Emscripten SDK anywhere a tool looks.
        static bool needsEmscriptenDownload(BuildTarget target, const Toolchains &toolchains);

        // The Haxe compile, the CMake configure and the CMake build. Empty when the target cannot be built on this machine, with the reason in error.
        static std::vector<BuildStep> steps(BuildTarget target, BuildType type, bool logToFile, const std::string &projectDirectory,
                                            const std::string &buildDirectory, const Toolchains &toolchains, std::string *error);

        // "Ready", or why the target cannot be built yet.
        static std::string status(BuildTarget target, const std::string &projectDirectory, const Toolchains &toolchains);

        // A step the way a shell takes it, so it can be pasted there.
        static std::string commandLine(const BuildStep &step);
    };
}
