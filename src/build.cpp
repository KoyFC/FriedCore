#include "fried/core/build.h"

#include "fried/core/engine_location.h"
#include "paths.h"

#include <algorithm>
#include <filesystem>

namespace fried::core
{
    namespace
    {
        namespace fs = std::filesystem;

        struct ConsoleToolchain
        {
            const char *m_variable;
            std::string m_sdk;
            std::string m_relativePath;
            std::string m_console;
        };

        ConsoleToolchain toolchainOf(Platform platform, const Toolchains &toolchains)
        {
            if (platform == Platform::Vita)
            {
                return {"VITASDK", toolchains.m_vitaSdk, "share/vita.toolchain.cmake", "Vita"};
            }
            if (platform == Platform::Switch)
            {
                return {"DEVKITPRO", toolchains.m_devkitPro, "cmake/Switch.cmake", "Switch"};
            }
            if (platform == Platform::NintendoDs)
            {
                return {"DEVKITPRO", toolchains.m_devkitPro, "cmake/NDS.cmake", "DS"};
            }
            return {"DEVKITPRO", toolchains.m_devkitPro, "cmake/3DS.cmake", "3DS"};
        }

        bool exists(const std::string &path)
        {
            std::error_code ignored;
            return fs::exists(pathOf(path), ignored);
        }

        std::string joined(const std::string &directory, const std::string &relative)
        {
            bool hasSeparator = !directory.empty() && (directory.back() == '/' || directory.back() == '\\');
            return directory + (hasSeparator ? "" : "/") + relative;
        }

        // The way the host's own tools write a path, which is what its users read and paste.
        std::string native(std::string path, HostSystem host)
        {
            if (host == HostSystem::Windows)
            {
                std::replace(path.begin(), path.end(), '/', '\\');
            }
            return path;
        }

        std::string consoleToolchainFile(Platform platform, const Toolchains &toolchains, std::string *error)
        {
            ConsoleToolchain toolchain = toolchainOf(platform, toolchains);
            if (toolchain.m_sdk.empty())
            {
                *error = std::string(toolchain.m_variable) + " is not set, so there is no " + toolchain.m_console + " toolchain to build with.";
                return std::string();
            }

            std::string file = joined(toolchain.m_sdk, toolchain.m_relativePath);
            if (!exists(file))
            {
                *error = std::string(toolchain.m_variable) + " points at " + native(toolchain.m_sdk, toolchains.m_host) + ", where there is no " +
                         toolchain.m_relativePath + ".";
                return std::string();
            }
            return file;
        }

        std::string fxsdkToolchainFile(const Toolchains &toolchains, std::string *error)
        {
            if (toolchains.m_fxsdk.empty())
            {
                *error = "fxsdk is not on the PATH, so there is no fx-CG50 toolchain to build with.";
                return std::string();
            }

            std::string file = textOf((pathOf(toolchains.m_fxsdk).parent_path() / ".." / "lib" / "cmake" / "fxsdk" / "FXCG50.cmake").lexically_normal());
            if (!exists(file))
            {
                *error = native(toolchains.m_fxsdk, toolchains.m_host) + " has no fx-CG50 toolchain at " + native(file, toolchains.m_host) + ".";
                return std::string();
            }
            return file;
        }

        std::string webToolchainFile(const Toolchains &toolchains, std::string *error)
        {
            if (toolchains.m_emscriptenSdk.empty())
            {
                *error = "There is no Emscripten SDK, so there is nothing to build for the web with.";
                return std::string();
            }
            return emscriptenToolchainFile(toolchains.m_emscriptenSdk);
        }

        // The engine ships this toolchain itself, so only the cross compiler has to be installed.
        std::string mingwToolchainFile(const std::string &projectDirectory, const Toolchains &toolchains, std::string *error)
        {
            if (toolchains.m_mingwCompiler.empty())
            {
                *error = "x86_64-w64-mingw32-g++ is not on the PATH, so there is no MinGW-w64 compiler to build for Windows with.";
                return std::string();
            }

            std::string engine = findEngine(projectDirectory);
            if (engine.empty())
            {
                *error = "There is no Fried Engine in the project's engine/ submodule, so there is no MinGW-w64 toolchain to build for Windows with.";
                return std::string();
            }

            std::string file = joined(engine, "cmake/toolchains/mingw-w64.cmake");
            if (!exists(file))
            {
                *error = "The project's engine has no MinGW-w64 toolchain at " + native(file, toolchains.m_host) +
                         ". Updating the engine submodule brings it.";
                return std::string();
            }
            return file;
        }

        std::vector<BuildStep> pipeline(const std::string &sourceDirectory, const std::string &buildTree, BuildType type,
                                        const std::vector<std::string> &configureExtras, const std::string &cmake = "cmake")
        {
            std::string typeName = buildTypeName(type);

            BuildStep configure;
            configure.m_arguments = {cmake, "-S", sourceDirectory, "-B", buildTree, "-DCMAKE_BUILD_TYPE=" + typeName};
            configure.m_arguments.insert(configure.m_arguments.end(), configureExtras.begin(), configureExtras.end());

            // A generator that holds every configuration at once ignores the variable and reads --config.
            BuildStep build;
            build.m_arguments = {cmake, "--build", buildTree, "--config", typeName, "--parallel"};

            return {configure, build};
        }

        // Absolute paths, since a tool's own PATH never has w64devkit on it, and its bin/ in front of the PATH for whatever the build looks up there.
        std::vector<BuildStep> w64devkitPipeline(const std::string &sourceDirectory, const std::string &buildTree, BuildType type,
                                                 const std::vector<std::string> &configureExtras, const Toolchains &toolchains)
        {
            auto tool = [&toolchains](const char *name) {
                return native(joined(toolchains.m_w64devkit, std::string("bin/") + name + ".exe"), HostSystem::Windows);
            };
            std::string cmake = tool("cmake");

            BuildStep configure;
            configure.m_arguments = {cmake,
                                     "-S",
                                     sourceDirectory,
                                     "-B",
                                     buildTree,
                                     "-G",
                                     "Ninja",
                                     "-DCMAKE_MAKE_PROGRAM=" + tool("ninja"),
                                     "-DCMAKE_C_COMPILER=" + tool("gcc"),
                                     "-DCMAKE_CXX_COMPILER=" + tool("g++"),
                                     "-DCMAKE_BUILD_TYPE=" + buildTypeName(type)};
            configure.m_arguments.insert(configure.m_arguments.end(), configureExtras.begin(), configureExtras.end());

            BuildStep build;
            build.m_arguments = {cmake, "--build", buildTree, "--parallel"};

            std::vector<BuildStep> steps = {configure, build};
            std::string path = native(joined(toolchains.m_w64devkit, "bin"), HostSystem::Windows) + ";" + toolchains.m_path;
            for (BuildStep &step : steps)
            {
                step.m_environment["PATH"] = path;
            }
            return steps;
        }

        std::vector<BuildStep> windowsPipeline(const std::string &sourceDirectory, const std::string &buildTree, BuildType type,
                                               const std::vector<std::string> &configureExtras, const Toolchains &toolchains, std::string *error)
        {
            if (!toolchains.m_visualStudioCmake.empty())
            {
                return pipeline(sourceDirectory, buildTree, type, configureExtras, toolchains.m_visualStudioCmake);
            }
            if (!toolchains.m_w64devkit.empty())
            {
                return w64devkitPipeline(sourceDirectory, buildTree, type, configureExtras, toolchains);
            }

            *error = "There is no C++ compiler to build with: Visual Studio's C++ tools are not installed, and neither is w64devkit.";
            return {};
        }

        const char *s_noCmake = "cmake is not on the PATH, so there is nothing to configure the build with.";

        // The Haxe compile and the steps after it, which run with the same PATH when w64devkit's tools are on it.
        std::vector<BuildStep> withHaxe(BuildType type, bool logToFile, std::vector<BuildStep> cmakeSteps)
        {
            if (cmakeSteps.empty())
            {
                return cmakeSteps;
            }
            BuildStep haxe = Build::haxeStep(type, logToFile);
            haxe.m_environment = cmakeSteps.front().m_environment;
            cmakeSteps.insert(cmakeSteps.begin(), haxe);
            return cmakeSteps;
        }
    }

    std::string buildTypeName(BuildType type)
    {
        return type == BuildType::Release ? "Release" : "Debug";
    }

    BuildType buildTypeNamed(const std::string &name)
    {
        return name == buildTypeName(BuildType::Release) ? BuildType::Release : BuildType::Debug;
    }

    std::string Build::defaultDirectory(BuildTarget target)
    {
        return target == BuildTarget::Pc ? "build" : "build/" + buildTargetKey(target);
    }

    BuildStep Build::haxeStep(BuildType type, bool logToFile)
    {
        BuildStep step;
        step.m_arguments = {"haxe", "build.hxml"};
        if (type == BuildType::Debug)
        {
            step.m_arguments.push_back("-debug");
        }
        if (logToFile)
        {
            step.m_arguments.push_back("-D");
            step.m_arguments.push_back("fried_log_file");
        }
        return step;
    }

    bool Build::needsCompilerDownload(BuildTarget target, const Toolchains &toolchains)
    {
        return toolchains.m_host == HostSystem::Windows && target == BuildTarget::Pc && toolchains.m_visualStudioCmake.empty() &&
               toolchains.m_w64devkit.empty();
    }

    bool Build::needsEmscriptenDownload(BuildTarget target, const Toolchains &toolchains)
    {
        return target == BuildTarget::Web && toolchains.m_emscriptenSdk.empty();
    }

    std::vector<BuildStep> Build::steps(BuildTarget target, BuildType type, bool logToFile, const std::string &projectDirectory,
                                        const std::string &buildDirectory, const Toolchains &toolchains, std::string *error)
    {
        if (buildDirectory.empty())
        {
            *error = "There is no build directory to build in.";
            return {};
        }

        if (toolchains.m_haxe.empty())
        {
            *error = "haxe is not on the PATH, so the game's Haxe cannot be compiled. Install Haxe and hxcpp.";
            return {};
        }

        std::string toolchain;
        switch (target)
        {
        case BuildTarget::Pc:
            return withHaxe(type, logToFile, Build::cmakeSteps(".", buildDirectory, type, {}, toolchains, error));
        case BuildTarget::WindowsFromHost:
            toolchain = mingwToolchainFile(projectDirectory, toolchains, error);
            break;
        case BuildTarget::Cg50:
            toolchain = fxsdkToolchainFile(toolchains, error);
            break;
        case BuildTarget::Web:
            toolchain = webToolchainFile(toolchains, error);
            break;
        default:
            toolchain = consoleToolchainFile(platformOf(target), toolchains, error);
        }
        if (toolchain.empty())
        {
            return {};
        }
        if (toolchains.m_cmake.empty())
        {
            *error = s_noCmake;
            return {};
        }
        return withHaxe(type, logToFile, pipeline(".", buildDirectory, type, {"-DCMAKE_TOOLCHAIN_FILE=" + toolchain}));
    }

    std::vector<BuildStep> Build::cmakeSteps(const std::string &sourceDirectory, const std::string &buildDirectory, BuildType type,
                                             const std::vector<std::string> &configureExtras, const Toolchains &toolchains, std::string *error)
    {
        if (toolchains.m_host == HostSystem::Windows)
        {
            return windowsPipeline(sourceDirectory, buildDirectory, type, configureExtras, toolchains, error);
        }
        if (toolchains.m_cmake.empty())
        {
            *error = s_noCmake;
            return {};
        }
        return pipeline(sourceDirectory, buildDirectory, type, configureExtras);
    }

    std::string Build::executablePath(const std::string &buildDirectory, const std::string &name, BuildType type, const Toolchains &toolchains)
    {
        if (toolchains.m_host != HostSystem::Windows)
        {
            return joined(buildDirectory, name);
        }
        if (!toolchains.m_visualStudioCmake.empty())
        {
            return joined(joined(buildDirectory, buildTypeName(type)), name + ".exe");
        }
        return joined(buildDirectory, name + ".exe");
    }

    std::string Build::status(BuildTarget target, const std::string &projectDirectory, const Toolchains &toolchains)
    {
        if (needsCompilerDownload(target, toolchains) || needsEmscriptenDownload(target, toolchains))
        {
            return "Not ready: the tool has to download a compiler first.";
        }

        std::string error;
        if (steps(target, BuildType::Debug, false, projectDirectory, defaultDirectory(target), toolchains, &error).empty())
        {
            return "Not ready: " + error;
        }
        return "Ready";
    }

    std::string Build::commandLine(const BuildStep &step)
    {
        std::string line;
        for (const std::string &argument : step.m_arguments)
        {
            bool needsQuotes = argument.empty() || argument.find(' ') != std::string::npos;
            line += (line.empty() ? "" : " ") + (needsQuotes ? "\"" + argument + "\"" : argument);
        }
        return line;
    }
}
