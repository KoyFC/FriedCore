#pragma once

#include <string>

namespace fried::core
{
    enum class LineKind
    {
        Plain,
        Command,
        Error,
        Warning,
        Progress,
        Success
    };

    struct OutputLine
    {
        std::string m_text;
        LineKind m_kind;
    };

    // What a line of a build's output is, by what the three tools in a build print: gcc and clang diagnostics, CMake's own messages and progress, and haxe, whose errors carry no word for what they are and are recognized by shape.
    LineKind kindOf(const std::string &line);
}
