#include "doctest/doctest.h"

#include "fried/core/toolchains.h"
#include "temporary_directory.h"

#include <cstdlib>
#include <filesystem>

#ifndef _WIN32

using namespace fried::core;

namespace
{
    // Points the manager's data folder at a temporary one, and puts the variable back.
    class ManagerData
    {
    public:
        ManagerData(const std::string &name)
            : m_directory(name)
        {
            const char *before = std::getenv("XDG_DATA_HOME");
            m_hadValue = before != nullptr;
            m_before = before ? before : "";
            setenv("XDG_DATA_HOME", m_directory.path().c_str(), 1);
        }

        ~ManagerData()
        {
            if (m_hadValue)
            {
                setenv("XDG_DATA_HOME", m_before.c_str(), 1);
            }
            else
            {
                unsetenv("XDG_DATA_HOME");
            }
        }

        void write(const std::string &relative) const { m_directory.write("Fried Engine/Fried Project Manager/tools/" + relative, ""); }
        std::string tools() const { return m_directory.path() + "/Fried Engine/Fried Project Manager/tools"; }

    private:
        TemporaryDirectory m_directory;
        std::string m_before;
        bool m_hadValue = false;
    };

    void writeW64devkit(const ManagerData &data, const std::string &folder, bool complete = true)
    {
        data.write(folder + "/bin/cmake.exe");
        if (complete)
        {
            data.write(folder + "/bin/ninja.exe");
            data.write(folder + "/bin/g++.exe");
        }
    }
}

TEST_CASE("the newest complete w64devkit the manager installed is the one found")
{
    ManagerData data("manager-w64devkit");
    writeW64devkit(data, "w64devkit-2.9.0");
    writeW64devkit(data, "w64devkit-2.10.1");
    writeW64devkit(data, "w64devkit-2.11.0-unpacking");
    writeW64devkit(data, "w64devkit-3.0.0", false);

    CHECK(managerTools().m_w64devkit == data.tools() + "/w64devkit-2.10.1");
}

TEST_CASE("an Emscripten SDK is found only with its toolchain file")
{
    ManagerData data("manager-emsdk");
    CHECK(managerTools().m_emscriptenSdk.empty());

    data.write("emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake");
    CHECK(managerTools().m_emscriptenSdk == data.tools() + "/emsdk");
}

TEST_CASE("nothing the manager installed gives empty folders")
{
    ManagerData data("manager-nothing");
    ManagedTools managed = managerTools();
    CHECK(managed.m_w64devkit.empty());
    CHECK(managed.m_emscriptenSdk.empty());
}

TEST_CASE("Visual Studio is looked for on Windows only")
{
    CHECK(visualStudioCmake().empty());
}

#endif
