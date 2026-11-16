# FriedCore

The logic that the tools of [Fried Engine](https://github.com/KoyFC/FriedEngine) share: the Fried Project Manager and the Fried Editor both link it, so what a project file means is written once. See [`ARCHITECTURE.md`](ARCHITECTURE.md) for what is in it.

It is a static C++17 library with no UI toolkit in it: the standard library and the vendored `nlohmann/json` and `stb` only. Nothing in it may include a Qt, Dear ImGui, SDL or GLFW header, and configuring fails if something does.

## Using it

A tool takes it as a submodule and adds it as a subdirectory, which builds the library without its tests:

```cmake
add_subdirectory(third_party/fried_core)
target_link_libraries(my_tool PRIVATE fried_core)
```

Headers are included as `fried/core/<name>.h`, and everything is in the `fried::core` namespace. Linking it also gives `nlohmann/json.hpp` and the `stb` image headers, whose implementations are compiled into the library, so a tool does not vendor a second copy or define them again.

## Building and testing it alone

```
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Layout

- `include/fried/core/` and `src/` are the library.
- `tests/` runs it with doctest. `tests/data/` holds project files as the tools write them.
- `third_party/` holds `nlohmann/json` 3.11.3, `doctest`, `stb_image` and `stb_image_write`, unchanged.
