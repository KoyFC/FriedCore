#pragma once

#include <filesystem>
#include <string>

namespace fried::core
{
    // Text in this library is UTF-8. A path made from a std::string is read in the system's code page on Windows, so every conversion goes through these.
    inline std::filesystem::path pathOf(const std::string &text)
    {
        return std::filesystem::u8path(text);
    }

    inline std::string textOf(const std::filesystem::path &path)
    {
        return path.generic_u8string();
    }
}
