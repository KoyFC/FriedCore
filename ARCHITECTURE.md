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

## Running a build

`process.h` starts a program without blocking the tool. Its output goes to a log file, or is kept for the tool to read as it arrives. The program leads a process group of its own, so stopping it also stops what it started, such as the compilers under a CMake build, and a program that ignores being asked is killed when the object that holds it goes away. It is written for POSIX systems only: the Project Manager runs its builds through Qt, and no other tool runs on Windows yet.

`step_runner.h` runs a list of programs one after another on top of that. A tool calls `pump` from its own loop and reads `lines`: each line of output with what it is (`kindOf` knows the errors, warnings and progress lines that haxe, CMake and the compilers print), preceded by the command that printed it. A step that fails stops the run, and `cancel` stops it at once.

## Images and icons

`image.h` is the image work a tool needs, on plain truecolor pixels: reading a PNG, JPEG or BMP, cutting the middle out of an image, scaling it, flattening it onto a color, and cutting its colors down to a palette. Scaling down averages the area each new pixel covers and scaling up interpolates, both weighted by alpha so a transparent edge does not bleed its color. The palette is a median cut, and an image that already has few enough colors keeps each of them. A photograph stored rotated is read as stored, since its orientation tag is not read.

It also writes the formats a console's packaging reads and the `stb` writers do not have: a PNG of one byte a pixel with a palette, and a BMP of sixteen colors. `writeFiles` writes several files or none: each is written beside its place and they are all moved into place only once every one was written.

`icon.h` turns one source image into each platform's icon, in the place that platform's build looks for it:

| Platform | File | What it is |
|---|---|---|
| PC | `assets/icon.png` | Square, truecolor, 512 pixels at most |
| Vita | `sce_sys/icon0.png` | 128x128 PNG with a palette of up to 256 colors, as the Vita's installer rejects a truecolor one |
| Switch | `switch/icon.jpg` | 256x256 JPEG, on black |
| 3DS | `3ds/icon.png` | 48x48 PNG, on black |
| DS | `nds/icon.bmp` | 32x32 BMP of sixteen colors, the first of them transparent |
| fx-CG50 | `cg50/icon-uns.png` and `cg50/icon-sel.png` | 92x64 PNGs of the same art inside a round frame, white unselected and blue selected |

The web has no icon. `Icon::render` gives the files ready to write, a preview of how the platform shows the icon, and notes that say what the source had to become, such as being cut to a square or approximated to fewer colors.

## Finding the engine

`engine_location.h` finds the engine a project builds with: its `engine/` submodule, or the folder above the project, which is where the engine's own sandbox sits. An engine is the folder that has `cmake/FriedEngine.cmake`.
