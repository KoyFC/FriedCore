#include "fried/core/android.h"

#include "paths.h"

#include <cctype>
#include <filesystem>
#include <regex>
#include <system_error>
#include <vector>

namespace fried::core::android
{
    namespace
    {
        namespace fs = std::filesystem;

        bool isFileAt(const fs::path &path)
        {
            std::error_code ignored;
            return fs::is_regular_file(path, ignored);
        }
    }

    std::string ndkToolchainFile(const std::string &ndk)
    {
        return textOf(pathOf(ndk) / "build" / "cmake" / "android.toolchain.cmake");
    }

    bool isNdk(const std::string &ndk)
    {
        return !ndk.empty() && isFileAt(pathOf(ndkToolchainFile(ndk)));
    }

    bool isSdk(const std::string &sdk)
    {
        return !sdk.empty() && isFileAt(pathOf(sdk) / "platforms" / s_platform / "android.jar") &&
               isFileAt(pathOf(sdk) / "build-tools" / s_buildTools / "aapt2");
    }

    bool isJdk(const std::string &home)
    {
        if (home.empty())
        {
            return false;
        }
        for (const char *tool : {"java", "javac", "jar", "keytool"})
        {
            if (!isFileAt(pathOf(home) / "bin" / tool))
            {
                return false;
            }
        }
        return true;
    }

    std::string managedSdkDirectory(const std::string &tools)
    {
        return textOf(pathOf(tools) / "android-sdk");
    }

    std::string managedNdkDirectory(const std::string &tools)
    {
        return textOf(pathOf(managedSdkDirectory(tools)) / "ndk" / s_ndkVersion);
    }

    std::string managedJdkDirectory(const std::string &tools)
    {
        return textOf(pathOf(tools) / "jdk-21");
    }

    std::string defaultPackage(const std::string &organization, const std::string &name)
    {
        std::vector<std::string> parts;
        for (const std::string &text : {std::string("com"), organization, name})
        {
            std::string part;
            for (char character : text)
            {
                char lower = (char)std::tolower((unsigned char)character);
                if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9') || lower == '_')
                {
                    part += lower;
                }
            }
            if (!part.empty() && (std::isdigit((unsigned char)part[0]) || part[0] == '_'))
            {
                part = "x" + part;
            }
            if (!part.empty())
            {
                parts.push_back(part);
            }
        }
        if (parts.size() < 2)
        {
            parts.push_back("game");
        }
        std::string joined;
        for (const std::string &part : parts)
        {
            joined += (joined.empty() ? "" : ".") + part;
        }
        return joined;
    }

    bool isValidPackage(const std::string &package)
    {
        static const std::regex s_package("^[a-zA-Z][a-zA-Z0-9_]*(\\.[a-zA-Z][a-zA-Z0-9_]*)+$");
        return std::regex_match(package, s_package);
    }

    std::string sanitizedPackage(const std::string &typed)
    {
        std::vector<std::string> parts;
        std::string part;
        auto finishPart = [&parts, &part]() {
            if (!part.empty())
            {
                if (std::isdigit((unsigned char)part[0]) || part[0] == '_')
                {
                    part = "x" + part;
                }
                parts.push_back(part);
            }
            part.clear();
        };
        for (char character : typed)
        {
            if (character == '.')
            {
                finishPart();
            }
            else if (std::isalnum((unsigned char)character) || character == '_')
            {
                part += character;
            }
        }
        finishPart();
        if (parts.empty())
        {
            return std::string();
        }
        if (parts.size() == 1)
        {
            parts.push_back("game");
        }
        std::string joined;
        for (const std::string &each : parts)
        {
            joined += (joined.empty() ? "" : ".") + each;
        }
        return joined;
    }
}
