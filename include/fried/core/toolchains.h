#pragma once

#include "fried/core/platform.h"

#include <string>

namespace fried::core
{
    // What the machine has to build with. A build's commands are worked out from this alone, so a test, or a tool on another system, can describe a machine by hand.
    struct Toolchains
    {
        HostSystem m_host = hostSystem();

        // What VITASDK and DEVKITPRO point at. Both SDKs export their variable from a shell profile, so a tool started from a desktop menu may never have inherited it.
        std::string m_vitaSdk;
        std::string m_devkitPro;

        // The fxsdk command. The fxSDK exports no variable of its own: it installs the command into <prefix>/bin and its CMake files under <prefix>/lib, so the command is what leads to the toolchain.
        std::string m_fxsdk;

        // The x86_64-w64-mingw32-g++ command, which builds for Windows from another system.
        std::string m_mingwCompiler;

        // The folder of an Emscripten SDK that holds its toolchain file.
        std::string m_emscriptenSdk;

        // The NDK, the SDK with the platform and build-tools the engine packages with, and a JDK's home, which an Android build needs all three of.
        std::string m_androidNdk;
        std::string m_androidSdk;
        std::string m_javaHome;

        // The adb that installs a build on a phone: the SDK's, or the one on the PATH. Empty when there is none, which only means a build cannot be installed.
        std::string m_adb;

        // On Windows, the CMake that drives Visual Studio's C++ tools, and the folder of a w64devkit with CMake, Ninja and g++ in it. Visual Studio is preferred when both are there.
        std::string m_visualStudioCmake;
        std::string m_w64devkit;

        // The haxe and cmake commands on the PATH. Every build runs haxe, and every one but Windows' PC build runs that cmake.
        std::string m_haxe;
        std::string m_cmake;

        // The PATH a build inherits, which a w64devkit build puts its own tools in front of.
        std::string m_path;
    };

    // The folders a tool installs toolchains of its own in, each empty when it installs none.
    struct ManagedTools
    {
        std::string m_emscriptenSdk;
        std::string m_w64devkit;
        std::string m_androidNdk;
        std::string m_androidSdk;
        std::string m_javaHome;
    };

    // The folder the Project Manager keeps the toolchains it downloads in, so another tool finds them there.
    std::string managerToolsDirectory();

    // The toolchains the Project Manager installed for itself: its Emscripten SDK, its newest w64devkit and its Android NDK, SDK and JDK, each empty when it is not there in full.
    ManagedTools managerTools();

    // The CMake that drives Visual Studio's C++ tools, found with vswhere. Empty on other systems, or with no Visual Studio that has them.
    std::string visualStudioCmake();

    std::string emscriptenToolchainFile(const std::string &sdkDirectory);

    // An executable on the PATH by its name, or an empty string. On Windows the name may leave out its extension.
    std::string findExecutable(const std::string &name);

    // Looks the machine over. Visual Studio is the one thing it does not look for, as asking for it runs a program: a tool that supports it sets m_visualStudioCmake from visualStudioCmake().
    Toolchains detectToolchains(const ManagedTools &managed);
}
