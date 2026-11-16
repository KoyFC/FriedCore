#pragma once

#include "fried/core/image.h"
#include "fried/core/platform.h"

#include <string>
#include <vector>

namespace fried::core
{
    // What a platform's icon files would hold for a source image.
    struct RenderedIcon
    {
        Platform m_platform = Platform::Pc;

        // The icon as the platform shows it, for a tool to draw.
        Image m_preview;

        // Ready to be written, with paths relative to the project's folder.
        std::vector<EncodedFile> m_files;

        // How many colors its palette has, for the platforms that store one.
        int m_colorCount = 0;

        bool isNull() const { return m_files.empty(); }
    };

    // A project keeps one icon per platform, because a window icon is truecolor of any size while each console reads one fixed size in a format of its own.
    class Icon
    {
    public:
        // Where the platform's build looks for its icon, relative to the project's folder. The fx-CG50 has two, unselected then selected, and the web has none.
        static std::vector<std::string> paths(Platform platform);

        // Saying in notes whatever the source had to become. Null for a null source or a platform with no icon.
        static RenderedIcon render(Platform platform, const Image &source, std::vector<std::string> *notes);

        // Writes every file or none. A null icon leaves that platform's files as they are.
        static bool write(const std::string &projectDirectory, const std::vector<RenderedIcon> &icons, std::string *error);

        // One sentence saying what was written.
        static std::string describe(const std::vector<RenderedIcon> &icons);
    };
}
