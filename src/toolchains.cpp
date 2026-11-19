#include "fried/core/toolchains.h"
#include "fried/core/process.h"
#include "paths.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <thread>
#include <vector>

namespace fried::core
{
    namespace
    {
        namespace fs = std::filesystem;

        const char *s_organization = "Fried Engine";
        const char *s_manager = "Fried Project Manager";

        std::string variable(const char *name)
        {
            const char *value = std::getenv(name);
            return value != nullptr ? std::string(value) : std::string();
        }

        std::vector<std::string> split(const std::string &text, char separator)
        {
            std::vector<std::string> parts;
            std::stringstream stream(text);
            std::string part;
            while (std::getline(stream, part, separator))
            {
                if (!part.empty())
                {
                    parts.push_back(part);
                }
            }
            return parts;
        }

        bool isFile(const fs::path &path)
        {
            std::error_code ignored;
            return fs::is_regular_file(path, ignored);
        }

        std::string homeDirectory()
        {
            std::string home = variable(hostSystem() == HostSystem::Windows ? "USERPROFILE" : "HOME");
            return home;
        }

        bool hasAllOf(const std::string &directory, std::initializer_list<const char *> tools)
        {
            for (const char *tool : tools)
            {
                if (!isFile(pathOf(directory) / "bin" / (std::string(tool) + ".exe")))
                {
                    return false;
                }
            }
            return true;
        }
    }

    // The same folder Qt gives the manager as its local data location.
    std::string managerToolsDirectory()
    {
        fs::path base;
        switch (hostSystem())
        {
        case HostSystem::Windows:
            base = variable("LOCALAPPDATA");
            break;
        case HostSystem::MacOs:
            base = pathOf(homeDirectory()) / "Library" / "Application Support";
            break;
        default:
        {
            std::string data = variable("XDG_DATA_HOME");
            base = data.empty() ? pathOf(homeDirectory()) / ".local" / "share" : pathOf(data);
        }
        }
        return textOf(base / s_organization / s_manager / "tools");
    }

    std::string emscriptenToolchainFile(const std::string &sdkDirectory)
    {
        return textOf(pathOf(sdkDirectory) / "upstream" / "emscripten" / "cmake" / "Modules" / "Platform" / "Emscripten.cmake");
    }

    std::string findExecutable(const std::string &name)
    {
        bool onWindows = hostSystem() == HostSystem::Windows;
        std::vector<std::string> extensions = {""};
        if (onWindows)
        {
            std::string declared = variable("PATHEXT");
            for (const std::string &extension : split(declared.empty() ? ".COM;.EXE;.BAT;.CMD" : declared, ';'))
            {
                extensions.push_back(extension);
            }
        }

        for (const std::string &directory : split(variable("PATH"), onWindows ? ';' : ':'))
        {
            for (const std::string &extension : extensions)
            {
                fs::path candidate = pathOf(directory) / (name + extension);
                if (isFile(candidate))
                {
                    return textOf(candidate);
                }
            }
        }
        return std::string();
    }

    Toolchains detectToolchains(const ManagedTools &managed)
    {
        Toolchains toolchains;
        toolchains.m_vitaSdk = variable("VITASDK");
        toolchains.m_devkitPro = variable("DEVKITPRO");
        toolchains.m_fxsdk = findExecutable("fxsdk");
        toolchains.m_mingwCompiler = findExecutable("x86_64-w64-mingw32-g++");
        toolchains.m_haxe = findExecutable("haxe");
        toolchains.m_cmake = findExecutable("cmake");
        toolchains.m_path = variable("PATH");

        // Each candidate has to hold the toolchain file, so a half-finished install is skipped.
        std::string home = homeDirectory();
        for (const std::string &candidate :
             {variable("EMSDK"), managed.m_emscriptenSdk, home.empty() ? std::string() : textOf(pathOf(home) / "emsdk")})
        {
            if (!candidate.empty() && isFile(pathOf(emscriptenToolchainFile(candidate))))
            {
                toolchains.m_emscriptenSdk = candidate;
                break;
            }
        }

        if (!managed.m_w64devkit.empty() && hasAllOf(managed.m_w64devkit, {"cmake", "ninja", "g++"}))
        {
            toolchains.m_w64devkit = managed.m_w64devkit;
        }
        return toolchains;
    }

    namespace
    {
        const std::string s_w64devkitPrefix = "w64devkit-";

        // The numbers of a folder's version, so 2.10.1 is newer than 2.9.0. Empty when the name is not a version.
        std::vector<int> versionOf(const std::string &name)
        {
            std::vector<int> numbers;
            for (const std::string &part : split(name, '.'))
            {
                if (part.find_first_not_of("0123456789") != std::string::npos)
                {
                    return {};
                }
                numbers.push_back(std::stoi(part));
            }
            return numbers;
        }
    }

    ManagedTools managerTools()
    {
        ManagedTools managed;
        fs::path tools = pathOf(managerToolsDirectory());

        if (isFile(pathOf(emscriptenToolchainFile(textOf(tools / "emsdk")))))
        {
            managed.m_emscriptenSdk = textOf(tools / "emsdk");
        }

        // A folder still being unpacked has a name that is not a version, so it is skipped with the rest.
        std::vector<int> newest;
        std::error_code ignored;
        for (const fs::directory_entry &entry : fs::directory_iterator(tools, ignored))
        {
            std::string name = entry.path().filename().string();
            if (name.compare(0, s_w64devkitPrefix.size(), s_w64devkitPrefix) != 0)
            {
                continue;
            }
            std::vector<int> version = versionOf(name.substr(s_w64devkitPrefix.size()));
            if (!version.empty() && version > newest && hasAllOf(textOf(entry.path()), {"cmake", "ninja", "g++"}))
            {
                newest = version;
                managed.m_w64devkit = textOf(entry.path());
            }
        }
        return managed;
    }

    std::string visualStudioCmake()
    {
        if (hostSystem() != HostSystem::Windows)
        {
            return std::string();
        }

        fs::path vswhere = pathOf(variable("ProgramFiles(x86)")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
        if (!isFile(vswhere))
        {
            return std::string();
        }

        SubprocessOptions query;
        query.m_arguments = {textOf(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                             "-property", "installationPath"};
        query.m_captureOutput = true;
        std::string error;
        std::unique_ptr<Subprocess> process = startSubprocess(query, &error);
        if (!process)
        {
            return std::string();
        }

        std::string output;
        auto began = std::chrono::steady_clock::now();
        while (process->isRunning() && std::chrono::steady_clock::now() - began < std::chrono::seconds(10))
        {
            output += process->readOutput();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        output += process->readOutput();
        if (process->isRunning() || process->exitCode() != 0)
        {
            return std::string();
        }

        std::string installation = output.substr(0, output.find_first_of("\r\n"));
        if (installation.empty())
        {
            return std::string();
        }

        std::string onPath = findExecutable("cmake");
        if (!onPath.empty())
        {
            return onPath;
        }

        // The CMake that Visual Studio's C++ workload installs for its own use.
        fs::path bundled = pathOf(installation) / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "CMake" / "bin" / "cmake.exe";
        return isFile(bundled) ? textOf(bundled) : std::string();
    }
}
