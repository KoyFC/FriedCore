#include "fried/core/engine_location.h"
#include "paths.h"

#include <filesystem>

namespace fried::core
{
    namespace fs = std::filesystem;

    std::string findEngine(const std::string &projectDirectory)
    {
        for (const fs::path &candidate : {pathOf(projectDirectory) / "engine", pathOf(projectDirectory) / ".."})
        {
            std::error_code ignored;
            fs::path normal = fs::weakly_canonical(candidate, ignored);
            if (fs::exists(normal / "cmake" / "FriedEngine.cmake", ignored))
            {
                return textOf(normal);
            }
        }
        return std::string();
    }
}
