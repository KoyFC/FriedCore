#include "fried/core/toolchains.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>
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
                if (!isFile(fs::path(directory) / "bin" / (std::string(tool) + ".exe")))
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
            base = fs::path(homeDirectory()) / "Library" / "Application Support";
            break;
        default:
        {
            std::string data = variable("XDG_DATA_HOME");
            base = data.empty() ? fs::path(homeDirectory()) / ".local" / "share" : fs::path(data);
        }
        }
        return (base / s_organization / s_manager / "tools").generic_string();
    }

    std::string emscriptenToolchainFile(const std::string &sdkDirectory)
    {
        return (fs::path(sdkDirectory) / "upstream" / "emscripten" / "cmake" / "Modules" / "Platform" / "Emscripten.cmake").generic_string();
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
                fs::path candidate = fs::path(directory) / (name + extension);
                if (isFile(candidate))
                {
                    return candidate.string();
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
        toolchains.m_path = variable("PATH");

        // Each candidate has to hold the toolchain file, so a half-finished install is skipped.
        std::string home = homeDirectory();
        for (const std::string &candidate :
             {variable("EMSDK"), managed.m_emscriptenSdk, home.empty() ? std::string() : (fs::path(home) / "emsdk").generic_string()})
        {
            if (!candidate.empty() && isFile(emscriptenToolchainFile(candidate)))
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
}
