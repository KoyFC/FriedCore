# Architecture

FriedCore holds what a tool needs to know about a Fried Engine project, with no UI in it. The engine's build is the authority on every rule here: `cmake/FriedEngine.cmake` and `cmake/ProjectDisplay.cmake` read `project.fried` when a game is built, and this library reads and writes the file the same way.

## Platforms and build targets

`platform.h` lists the platforms a game runs on, with the key the engine gives each one (`pc`, `vita`, `switch`, `3ds`, `nds`, `cg50`, `web`), which is also the name of its section in `project.fried`.

`build_target.h` lists what a build compiles for. It is the platforms plus Windows built from another system, which is a target of its own whose game is still the PC platform. On Windows that target is not offered, as the PC target already is Windows. The functions take the host system as an argument, so a test can ask what another system offers.

## The project file

`project.h` reads and writes `project.fried`. `Project` keeps the whole JSON document, so a key it does not know survives a save. Setting a value to an empty string removes its key, which the engine reads as undeclared, and a section left with no keys is removed with its last one.

The file is written with tabs and its keys in alphabetical order, the same bytes the Project Manager has always written, so a save that changes nothing leaves no difference. It is written beside the file and moved over it, so a failed write leaves the project as it was.

The display follows `ProjectDisplay.cmake`: a project with no `display` uses the `default` mode, which has no size. `s_suggestedDisplay` is only what a tool proposes when a project leaves that mode. A display inside a platform's section is no longer read by the engine, which refuses to build with one, and `dropPlatformDisplays` takes them out.

`checkIdentity`, `checkVersion` and `checkVitaTitleId` say what is wrong with a value, and the limits of the 3DS and DS packaging text are constants of `Project`. A tool checks a value before setting it: the setters and `save` accept anything.

`logicalSize` gives the size a game draws in on a platform's own screen for the project's display mode, which is the area a camera shows there.

## Builds

`toolchains.h` describes what a machine has to build with as plain data, a `Toolchains`: where the Vita SDK and devkitPro are, the `fxsdk` and MinGW-w64 commands, an Emscripten SDK, and on Windows the CMake that drives Visual Studio and a w64devkit. `detectToolchains` fills one from the environment, the `PATH` and the folders a tool installs toolchains of its own in. Visual Studio is the one thing it does not look for, because asking for it runs a program: a tool that supports it sets that field itself. `managerToolsDirectory` is the folder the Project Manager downloads toolchains into, so the editor finds an Emscripten SDK the manager installed.

`build.h` turns a target, a build type and a `Toolchains` into the steps of a build: the Haxe compile, the CMake configure and the CMake build, each a list of arguments run in the project's folder. `Build::steps` reads nothing but its arguments and whether the toolchain files exist, so the commands of a Windows build are tested on Linux by describing a Windows machine. When a target cannot be built it returns no steps and says what is missing, and `Build::status` is that sentence, or "Ready", for a tool to show.

On Windows the PC build uses Visual Studio's CMake when there is one. Otherwise it uses w64devkit by the full path of each tool, with its `bin` folder put in front of the `PATH` of every step, as a tool's own `PATH` never has it. Installing w64devkit or the Emscripten SDK is the Project Manager's job and is not in this library.

## Finding the engine

`engine_location.h` finds the engine a project builds with: its `engine/` submodule, or the folder above the project, which is where the engine's own sandbox sits. An engine is the folder that has `cmake/FriedEngine.cmake`.
