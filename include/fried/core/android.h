#pragma once

#include <string>
#include <vector>

namespace fried::core::android
{
    // What an Android build compiles and packages with, which the Project Manager downloads and the engine's cmake/Android.cmake expects.
    inline constexpr const char *s_ndkVersion = "27.3.13750724";
    inline constexpr const char *s_platform = "android-35";
    inline constexpr const char *s_buildTools = "35.0.1";
    inline constexpr const char *s_minPlatform = "android-24";
    inline constexpr const char *s_abi = "arm64-v8a";

    std::string ndkToolchainFile(const std::string &ndk);
    bool isNdk(const std::string &ndk);
    // Holds the platform and the build-tools above.
    bool isSdk(const std::string &sdk);
    // Holds bin/java, bin/javac, bin/jar and bin/keytool.
    bool isJdk(const std::string &home);

    // The adb of an SDK's platform-tools, which installs a build on a phone. Empty when the SDK has none.
    std::string adbIn(const std::string &sdk);

    // A device `adb devices` listed. Only the state "device" is ready to install on: "unauthorized" waits for the phone's screen to be tapped, and "offline" for the cable.
    struct Device
    {
        std::string m_serial;
        std::string m_state;
        bool isReady() const { return m_state == "device"; }
    };

    // Reads what `adb devices` printed, one device per line after its header, the way a shell would show it.
    std::vector<Device> parseDevices(const std::string &output);

    // The newest .apk straight inside a build folder, or empty. The target's name is the project's, so the folder is what is known.
    std::string findApk(const std::string &buildDirectory);

    // The package a project is installed as: what it declares, or the one defaultPackage() makes.
    std::string packageOf(const std::string &declared, const std::string &organization, const std::string &name);

    // Where the Project Manager unpacks each, inside its tools folder.
    std::string managedSdkDirectory(const std::string &tools);
    std::string managedNdkDirectory(const std::string &tools);
    std::string managedJdkDirectory(const std::string &tools);

    // com.<organization>.<name>, lowercased with everything but letters, digits and underscores taken out, the way cmake/Android.cmake names a project that gives no "android.package".
    std::string defaultPackage(const std::string &organization, const std::string &name);
    bool isValidPackage(const std::string &package);
    // The nearest package Android takes to what was typed: lowercase is kept as typed, characters other than letters, digits, underscores and dots are dropped, empty parts are dropped, a part starting with a digit or an underscore gets an x in front, and one part alone gets ".game" after it. Empty stays empty, which means the default.
    std::string sanitizedPackage(const std::string &typed);
}
