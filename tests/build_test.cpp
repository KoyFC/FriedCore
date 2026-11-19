#include "doctest/doctest.h"

#include "fried/core/build.h"
#include "temporary_directory.h"

#include <cstdlib>
#include <filesystem>

using namespace fried::core;

namespace
{
    using Commands = std::vector<std::vector<std::string>>;

    Commands commandsOf(const std::vector<BuildStep> &steps)
    {
        Commands commands;
        for (const BuildStep &step : steps)
        {
            commands.push_back(step.m_arguments);
        }
        return commands;
    }

    Toolchains linuxMachine()
    {
        Toolchains toolchains;
        toolchains.m_host = HostSystem::Linux;
        return toolchains;
    }

    Toolchains windowsMachine()
    {
        Toolchains toolchains;
        toolchains.m_host = HostSystem::Windows;
        toolchains.m_path = "C:\\Windows";
        return toolchains;
    }
}

TEST_CASE("a PC build compiles the Haxe side, configures and builds")
{
    std::string error;
    Commands debug = commandsOf(Build::steps(BuildTarget::Pc, BuildType::Debug, false, "/game", "build", linuxMachine(), &error));
    CHECK(debug == Commands{{"haxe", "build.hxml", "-debug"},
                            {"cmake", "-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=Debug"},
                            {"cmake", "--build", "build", "--config", "Debug", "--parallel"}});

    Commands release = commandsOf(Build::steps(BuildTarget::Pc, BuildType::Release, true, "/game", "out dir", linuxMachine(), &error));
    CHECK(release == Commands{{"haxe", "build.hxml", "-D", "fried_log_file"},
                              {"cmake", "-S", ".", "-B", "out dir", "-DCMAKE_BUILD_TYPE=Release"},
                              {"cmake", "--build", "out dir", "--config", "Release", "--parallel"}});
    CHECK(error.empty());
}

TEST_CASE("a build with no directory is refused")
{
    std::string error;
    CHECK(Build::steps(BuildTarget::Pc, BuildType::Debug, false, "/game", "", linuxMachine(), &error).empty());
    CHECK(error == "There is no build directory to build in.");
}

TEST_CASE("each target has a build directory of its own")
{
    CHECK(Build::defaultDirectory(BuildTarget::Pc) == "build");
    CHECK(Build::defaultDirectory(BuildTarget::WindowsFromHost) == "build/windows");
    CHECK(Build::defaultDirectory(BuildTarget::Nintendo3ds) == "build/3ds");
    CHECK(Build::defaultDirectory(BuildTarget::Web) == "build/web");
}

TEST_CASE("a console build configures with its SDK's toolchain file")
{
    TemporaryDirectory sdks("build-consoles");
    sdks.write("vitasdk/share/vita.toolchain.cmake", "");
    sdks.write("devkitpro/cmake/Switch.cmake", "");
    sdks.write("devkitpro/cmake/3DS.cmake", "");
    sdks.write("devkitpro/cmake/NDS.cmake", "");

    Toolchains toolchains = linuxMachine();
    toolchains.m_vitaSdk = sdks.path() + "/vitasdk";
    toolchains.m_devkitPro = sdks.path() + "/devkitpro";

    auto toolchainOf = [&toolchains](BuildTarget target) {
        std::string error;
        std::vector<BuildStep> steps = Build::steps(target, BuildType::Release, false, "/game", Build::defaultDirectory(target), toolchains, &error);
        REQUIRE(steps.size() == 3);
        CHECK(steps[1].m_arguments.size() == 7);
        return steps[1].m_arguments.back();
    };
    CHECK(toolchainOf(BuildTarget::Vita) == "-DCMAKE_TOOLCHAIN_FILE=" + sdks.path() + "/vitasdk/share/vita.toolchain.cmake");
    CHECK(toolchainOf(BuildTarget::Switch) == "-DCMAKE_TOOLCHAIN_FILE=" + sdks.path() + "/devkitpro/cmake/Switch.cmake");
    CHECK(toolchainOf(BuildTarget::Nintendo3ds) == "-DCMAKE_TOOLCHAIN_FILE=" + sdks.path() + "/devkitpro/cmake/3DS.cmake");
    CHECK(toolchainOf(BuildTarget::NintendoDs) == "-DCMAKE_TOOLCHAIN_FILE=" + sdks.path() + "/devkitpro/cmake/NDS.cmake");
}

TEST_CASE("a console with no SDK, or one without its toolchain, says what is missing")
{
    Toolchains toolchains = linuxMachine();
    std::string error;
    CHECK(Build::steps(BuildTarget::Vita, BuildType::Debug, false, "/game", "build/vita", toolchains, &error).empty());
    CHECK(error == "VITASDK is not set, so there is no Vita toolchain to build with.");

    toolchains.m_devkitPro = "/nonexistent/devkitpro";
    CHECK(Build::steps(BuildTarget::NintendoDs, BuildType::Debug, false, "/game", "build/nds", toolchains, &error).empty());
    CHECK(error == "DEVKITPRO points at /nonexistent/devkitpro, where there is no cmake/NDS.cmake.");
    CHECK(Build::status(BuildTarget::NintendoDs, "/game", toolchains) == "Not ready: " + error);
    CHECK(Build::status(BuildTarget::Pc, "/game", toolchains) == "Ready");
}

TEST_CASE("the fx-CG50 toolchain is found from the fxsdk command")
{
    TemporaryDirectory prefix("build-fxsdk");
    prefix.write("bin/fxsdk", "");
    prefix.write("lib/cmake/fxsdk/FXCG50.cmake", "");

    Toolchains toolchains = linuxMachine();
    std::string error;
    CHECK(Build::steps(BuildTarget::Cg50, BuildType::Debug, false, "/game", "build/cg50", toolchains, &error).empty());
    CHECK(error == "fxsdk is not on the PATH, so there is no fx-CG50 toolchain to build with.");

    toolchains.m_fxsdk = prefix.path() + "/bin/fxsdk";
    std::vector<BuildStep> steps = Build::steps(BuildTarget::Cg50, BuildType::Debug, false, "/game", "build/cg50", toolchains, &error);
    REQUIRE(steps.size() == 3);
    CHECK(steps[1].m_arguments.back() == "-DCMAKE_TOOLCHAIN_FILE=" + prefix.path() + "/lib/cmake/fxsdk/FXCG50.cmake");
}

TEST_CASE("the web build uses the Emscripten SDK, or asks for it to be downloaded")
{
    Toolchains toolchains = linuxMachine();
    std::string error;
    CHECK(Build::needsEmscriptenDownload(BuildTarget::Web, toolchains));
    CHECK_FALSE(Build::needsEmscriptenDownload(BuildTarget::Pc, toolchains));
    CHECK(Build::steps(BuildTarget::Web, BuildType::Debug, false, "/game", "build/web", toolchains, &error).empty());
    CHECK(error == "There is no Emscripten SDK, so there is nothing to build for the web with.");
    CHECK(Build::status(BuildTarget::Web, "/game", toolchains) == "Not ready: the tool has to download a compiler first.");

    toolchains.m_emscriptenSdk = "/opt/emsdk";
    std::vector<BuildStep> steps = Build::steps(BuildTarget::Web, BuildType::Debug, false, "/game", "build/web", toolchains, &error);
    REQUIRE(steps.size() == 3);
    CHECK(steps[1].m_arguments.back() == "-DCMAKE_TOOLCHAIN_FILE=/opt/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake");
}

TEST_CASE("Windows from another system needs the cross compiler and the engine's toolchain file")
{
    TemporaryDirectory root("build-mingw");
    root.write("game/project.fried", "{}");
    root.write("modern/engine/cmake/FriedEngine.cmake", "");
    root.write("modern/engine/cmake/toolchains/mingw-w64.cmake", "");
    root.write("old/engine/cmake/FriedEngine.cmake", "");
    std::string base = std::filesystem::weakly_canonical(root.path()).string();

    Toolchains toolchains = linuxMachine();
    std::string error;
    CHECK(Build::steps(BuildTarget::WindowsFromHost, BuildType::Debug, false, root.path() + "/modern", "build/windows", toolchains, &error).empty());
    CHECK(error == "x86_64-w64-mingw32-g++ is not on the PATH, so there is no MinGW-w64 compiler to build for Windows with.");

    toolchains.m_mingwCompiler = "/usr/bin/x86_64-w64-mingw32-g++";
    CHECK(Build::steps(BuildTarget::WindowsFromHost, BuildType::Debug, false, root.path() + "/game", "build/windows", toolchains, &error).empty());
    CHECK(error.find("There is no Fried Engine") == 0);

    CHECK(Build::steps(BuildTarget::WindowsFromHost, BuildType::Debug, false, root.path() + "/old", "build/windows", toolchains, &error).empty());
    CHECK(error == "The project's engine has no MinGW-w64 toolchain at " + base + "/old/engine/cmake/toolchains/mingw-w64.cmake. Updating the engine submodule brings it.");

    std::vector<BuildStep> steps =
        Build::steps(BuildTarget::WindowsFromHost, BuildType::Debug, false, root.path() + "/modern", "build/windows", toolchains, &error);
    REQUIRE(steps.size() == 3);
    CHECK(steps[1].m_arguments.back() == "-DCMAKE_TOOLCHAIN_FILE=" + base + "/modern/engine/cmake/toolchains/mingw-w64.cmake");
}

TEST_CASE("on Windows the PC build uses Visual Studio's CMake when there is one")
{
    Toolchains toolchains = windowsMachine();
    toolchains.m_visualStudioCmake = "C:\\VS\\cmake.exe";
    toolchains.m_w64devkit = "C:/tools/w64devkit-2.10.1";

    std::string error;
    std::vector<BuildStep> steps = Build::steps(BuildTarget::Pc, BuildType::Release, false, "C:/game", "build", toolchains, &error);
    CHECK(commandsOf(steps) == Commands{{"haxe", "build.hxml"},
                                        {"C:\\VS\\cmake.exe", "-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=Release"},
                                        {"C:\\VS\\cmake.exe", "--build", "build", "--config", "Release", "--parallel"}});
    CHECK(steps[0].m_environment.empty());
    CHECK_FALSE(Build::needsCompilerDownload(BuildTarget::Pc, toolchains));
}

TEST_CASE("on Windows without Visual Studio the PC build uses w64devkit by its full paths")
{
    Toolchains toolchains = windowsMachine();
    toolchains.m_w64devkit = "C:/tools/w64devkit-2.10.1";

    std::string error;
    std::vector<BuildStep> steps = Build::steps(BuildTarget::Pc, BuildType::Debug, false, "C:/game", "build", toolchains, &error);
    std::string bin = "C:\\tools\\w64devkit-2.10.1\\bin\\";
    CHECK(commandsOf(steps) == Commands{{"haxe", "build.hxml", "-debug"},
                                        {bin + "cmake.exe", "-S", ".", "-B", "build", "-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=" + bin + "ninja.exe",
                                         "-DCMAKE_C_COMPILER=" + bin + "gcc.exe", "-DCMAKE_CXX_COMPILER=" + bin + "g++.exe", "-DCMAKE_BUILD_TYPE=Debug"},
                                        {bin + "cmake.exe", "--build", "build", "--parallel"}});
    for (const BuildStep &step : steps)
    {
        CHECK(step.m_environment.at("PATH") == "C:\\tools\\w64devkit-2.10.1\\bin;C:\\Windows");
    }
}

TEST_CASE("on Windows with no compiler at all the PC build asks for one to be downloaded")
{
    Toolchains toolchains = windowsMachine();
    std::string error;
    CHECK(Build::steps(BuildTarget::Pc, BuildType::Debug, false, "C:/game", "build", toolchains, &error).empty());
    CHECK(error == "There is no C++ compiler to build with: Visual Studio's C++ tools are not installed, and neither is w64devkit.");
    CHECK(Build::needsCompilerDownload(BuildTarget::Pc, toolchains));
    CHECK_FALSE(Build::needsCompilerDownload(BuildTarget::Vita, toolchains));
    CHECK_FALSE(Build::needsCompilerDownload(BuildTarget::Pc, linuxMachine()));
}

TEST_CASE("any CMake project configures from its own folder")
{
    std::string error;
    Commands plain = commandsOf(Build::cmakeSteps("/engine/editor/player", "/engine/editor/player/build", BuildType::Release, {}, linuxMachine(), &error));
    CHECK(plain == Commands{{"cmake", "-S", "/engine/editor/player", "-B", "/engine/editor/player/build", "-DCMAKE_BUILD_TYPE=Release"},
                            {"cmake", "--build", "/engine/editor/player/build", "--config", "Release", "--parallel"}});

    Commands extra = commandsOf(Build::cmakeSteps("/engine/editor/project-player", "/game/build/editor-player", BuildType::Release,
                                                  {"-DFRIED_PROJECT_DIR=/game"}, linuxMachine(), &error));
    CHECK(extra[0].back() == "-DFRIED_PROJECT_DIR=/game");
    CHECK(error.empty());
}

TEST_CASE("any CMake project builds with Visual Studio or w64devkit on Windows, or asks for a compiler")
{
    std::string error;
    Toolchains visualStudio = windowsMachine();
    visualStudio.m_visualStudioCmake = "C:\\VS\\cmake.exe";
    std::vector<BuildStep> steps = Build::cmakeSteps("C:/engine/editor/player", "C:/engine/editor/player/build", BuildType::Release, {"-DX=1"}, visualStudio, &error);
    CHECK(commandsOf(steps) == Commands{{"C:\\VS\\cmake.exe", "-S", "C:/engine/editor/player", "-B", "C:/engine/editor/player/build", "-DCMAKE_BUILD_TYPE=Release", "-DX=1"},
                                        {"C:\\VS\\cmake.exe", "--build", "C:/engine/editor/player/build", "--config", "Release", "--parallel"}});

    Toolchains w64devkit = windowsMachine();
    w64devkit.m_w64devkit = "C:/tools/w64devkit-2.10.1";
    steps = Build::cmakeSteps("C:/engine/editor/player", "build", BuildType::Debug, {"-DX=1"}, w64devkit, &error);
    REQUIRE(steps.size() == 2);
    CHECK(steps[0].m_arguments[2] == "C:/engine/editor/player");
    CHECK(steps[0].m_arguments.back() == "-DX=1");
    CHECK(steps[1].m_environment.at("PATH") == "C:\\tools\\w64devkit-2.10.1\\bin;C:\\Windows");

    CHECK(Build::cmakeSteps(".", "build", BuildType::Debug, {}, windowsMachine(), &error).empty());
    CHECK(error == "There is no C++ compiler to build with: Visual Studio's C++ tools are not installed, and neither is w64devkit.");
}

TEST_CASE("an executable is found where the build leaves it")
{
    Toolchains visualStudio = windowsMachine();
    visualStudio.m_visualStudioCmake = "C:\\VS\\cmake.exe";
    Toolchains w64devkit = windowsMachine();
    w64devkit.m_w64devkit = "C:/tools/w64devkit-2.10.1";

    CHECK(Build::executablePath("/game/build", "my_game", BuildType::Debug, linuxMachine()) == "/game/build/my_game");
    CHECK(Build::executablePath("C:/game/build", "my_game", BuildType::Debug, visualStudio) == "C:/game/build/Debug/my_game.exe");
    CHECK(Build::executablePath("C:/game/build/", "my_game", BuildType::Release, w64devkit) == "C:/game/build/my_game.exe");
}

TEST_CASE("a step is shown the way a shell takes it")
{
    BuildStep step;
    step.m_arguments = {"cmake", "-B", "out dir", ""};
    CHECK(Build::commandLine(step) == "cmake -B \"out dir\" \"\"");
}

#ifndef _WIN32
TEST_CASE("the machine is looked over through its environment and its PATH")
{
    TemporaryDirectory machine("build-detect");
    machine.write("bin/fxsdk", "");
    machine.write("bin/notes.txt", "");
    machine.write("managed-emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake", "");
    machine.write("half-emsdk/readme", "");

    std::string oldPath = std::getenv("PATH");
    setenv("PATH", (machine.path() + "/bin:" + oldPath).c_str(), 1);
    setenv("VITASDK", "/opt/vitasdk", 1);
    setenv("EMSDK", (machine.path() + "/half-emsdk").c_str(), 1);

    ManagedTools managed;
    managed.m_emscriptenSdk = machine.path() + "/managed-emsdk";
    managed.m_w64devkit = machine.path() + "/no-w64devkit";
    Toolchains toolchains = detectToolchains(managed);

    CHECK(toolchains.m_vitaSdk == "/opt/vitasdk");
    CHECK(toolchains.m_fxsdk == machine.path() + "/bin/fxsdk");
    CHECK(toolchains.m_emscriptenSdk == machine.path() + "/managed-emsdk");
    CHECK(toolchains.m_w64devkit.empty());
    CHECK(findExecutable("fxsdk") == machine.path() + "/bin/fxsdk");
    CHECK(findExecutable("no-such-program-here").empty());

    setenv("PATH", oldPath.c_str(), 1);
    unsetenv("VITASDK");
    unsetenv("EMSDK");
}
#endif
