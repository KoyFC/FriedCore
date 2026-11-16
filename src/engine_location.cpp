#include "fried/core/engine_location.h"

#include <filesystem>

namespace fried::core
{
    namespace fs = std::filesystem;

    std::string findEngine(const std::string &projectDirectory)
    {
        for (const fs::path &candidate : {fs::path(projectDirectory) / "engine", fs::path(projectDirectory) / ".."})
        {
            std::error_code ignored;
            fs::path normal = fs::weakly_canonical(candidate, ignored);
            if (fs::exists(normal / "cmake" / "FriedEngine.cmake", ignored))
            {
                return normal.string();
            }
        }
        return std::string();
    }
}
