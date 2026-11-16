#include "fried/core/line_kind.h"

#include <cctype>

namespace fried::core
{
    namespace
    {
        bool contains(const std::string &text, const char *part)
        {
            return text.find(part) != std::string::npos;
        }

        bool startsWith(const std::string &text, const char *part)
        {
            return text.rfind(part, 0) == 0;
        }

        std::string lowered(std::string text)
        {
            for (char &character : text)
            {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
            return text;
        }

        // "<no spaces>.hx:<digits>: " at the start of the line.
        bool isHaxeDiagnostic(const std::string &line)
        {
            std::size_t marker = line.find(".hx:");
            if (marker == std::string::npos || marker == 0 || line.find_first_of(" \t") < marker)
            {
                return false;
            }
            std::size_t digit = marker + 4;
            std::size_t end = digit;
            while (end < line.size() && std::isdigit(static_cast<unsigned char>(line[end])))
            {
                ++end;
            }
            return end > digit && line.compare(end, 2, ": ") == 0;
        }
    }

    LineKind kindOf(const std::string &line)
    {
        std::string lower = lowered(line);
        if (contains(lower, "error:") || contains(line, "CMake Error") || contains(line, "undefined reference to") || isHaxeDiagnostic(line))
        {
            return LineKind::Error;
        }
        if (contains(lower, "warning:") || contains(line, "CMake Warning") || startsWith(line, "Warning :"))
        {
            return LineKind::Warning;
        }

        // make's percentage, and CMake's own status lines.
        if (startsWith(line, "[") || startsWith(line, "-- "))
        {
            return LineKind::Progress;
        }
        return LineKind::Plain;
    }
}
