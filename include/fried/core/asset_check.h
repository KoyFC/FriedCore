#pragma once

#include "fried/core/platform.h"

#include <string>
#include <vector>

namespace fried::core
{
    // A way a project's asset does not suit a platform, and how it can be put right.
    struct AssetIssue
    {
        enum class Severity
        {
            Warning,
            Error
        };

        enum class Fix
        {
            None,
            ShrinkImage,
            DownmixWav,
            PaletteImage
        };

        Severity m_severity = Severity::Warning;

        // "game/sprite.png" for one of the assets, "sce_sys/icon0.png" for a file the platform's package takes from the project's folder, or empty for something about the project as a whole.
        std::string m_asset;
        std::string m_message;
        Fix m_fix = Fix::None;
        std::string m_fixLabel;
    };

    // Checks the project's own assets (the engine's are the engine's), and the files its package for the platform is made of, against the limits of a platform.
    std::vector<AssetIssue> validateAssets(Platform platform, const std::string &projectDirectory);

    // Puts an issue right. The file as it was is kept under the backup directory, at the same relative path, so nothing is lost.
    bool applyFix(const AssetIssue &issue, const std::string &projectDirectory, const std::string &backupDirectory, std::string *error);
}
