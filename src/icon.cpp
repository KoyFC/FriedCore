#include "fried/core/icon.h"

#include <algorithm>
#include <cmath>

namespace fried::core
{
    namespace
    {
        const char *s_pcPath = "assets/icon.png";
        const char *s_vitaPath = "sce_sys/icon0.png";
        const char *s_switchPath = "switch/icon.jpg";
        const char *s_nintendo3dsPath = "3ds/icon.png";
        const char *s_nintendoDsPath = "nds/icon.bmp";
        const char *s_cg50Path = "cg50/icon-uns.png";
        const char *s_cg50SelectedPath = "cg50/icon-sel.png";
        const char *s_androidPath = "android/icon.png";

        constexpr int s_vitaIconSize = 128;
        constexpr int s_switchIconSize = 256;
        constexpr int s_nintendo3dsIconSize = 48;
        constexpr int s_nintendoDsIconSize = 32;
        // The launcher's largest density, which Android scales down for the others.
        constexpr int s_androidIconSize = 192;
        // The DS menu's icon has sixteen colors, the first of them transparent.
        constexpr int s_nintendoDsColors = 16;
        constexpr int s_paletteColorLimit = 256;
        constexpr int s_largestWindowIcon = 512;

        // A .g3a's menu icon, drawn inside a frame of the calculator's own: white around it unselected and blue selected, the way the fxSDK's template draws its pair, so the menu shows which one is chosen.
        constexpr int s_cg50IconWidth = 92;
        constexpr int s_cg50IconHeight = 64;
        constexpr int s_cg50FrameWidth = 5;
        constexpr double s_cg50FrameRadius = 5.0;
        constexpr Color s_cg50SelectedFrame = {99, 207, 255, 255};
        constexpr Color s_white = {255, 255, 255, 255};
        constexpr Color s_black = {0, 0, 0, 255};

        std::string sizeText(int size)
        {
            return std::to_string(size) + "x" + std::to_string(size);
        }

        // An icon is square wherever it is shown, so an oblong source keeps its middle.
        Image squared(const Image &image, std::vector<std::string> *notes)
        {
            return cropped(image, 1, 1, notes);
        }

        Image flattenedOntoBlack(const Image &image, const std::string &reason, std::vector<std::string> *notes)
        {
            if (image.m_hasAlpha)
            {
                notes->push_back("Flattened onto black, since " + reason + ".");
            }
            return flattened(image, s_black);
        }

        RenderedIcon forPc(const Image &source, std::vector<std::string> *notes)
        {
            Image pc = squared(source, notes);
            if (pc.m_width > s_largestWindowIcon)
            {
                notes->push_back("Reduced from " + sizeText(pc.m_width) + " to " + sizeText(s_largestWindowIcon) + ".");
                pc = scaled(pc, s_largestWindowIcon, s_largestWindowIcon);
            }

            RenderedIcon icon;
            icon.m_files = {{s_pcPath, encodePng(pc)}};
            icon.m_preview = std::move(pc);
            return icon;
        }

        RenderedIcon forAndroid(const Image &source, std::vector<std::string> *notes)
        {
            Image sized = resized(squared(source, notes), s_androidIconSize, s_androidIconSize, notes);
            RenderedIcon icon;
            icon.m_files = {{s_androidPath, encodePng(sized)}};
            icon.m_preview = std::move(sized);
            return icon;
        }

        // The Vita's installer rejects a truecolor PNG wherever it reads one.
        RenderedIcon forVita(const Image &source, std::vector<std::string> *notes)
        {
            Image sized = resized(squared(source, notes), s_vitaIconSize, s_vitaIconSize, notes);
            bool wasApproximated = false;
            IndexedImage indexed = quantized(sized, s_paletteColorLimit, false, &wasApproximated);
            if (wasApproximated)
            {
                notes->push_back("Approximated to " + std::to_string(s_paletteColorLimit) + " colors, which is all a palette PNG holds.");
            }

            RenderedIcon icon;
            icon.m_files = {{s_vitaPath, encodePalettePng(indexed)}};
            icon.m_preview = indexed.toImage();
            icon.m_colorCount = static_cast<int>(indexed.m_palette.size());
            return icon;
        }

        RenderedIcon forSwitch(const Image &source, std::vector<std::string> *notes)
        {
            Image sized = resized(squared(source, notes), s_switchIconSize, s_switchIconSize, notes);
            RenderedIcon icon;
            icon.m_preview = flattenedOntoBlack(sized, "a JPEG holds no transparency", notes);
            icon.m_files = {{s_switchPath, encodeJpeg(icon.m_preview)}};
            return icon;
        }

        RenderedIcon forNintendo3ds(const Image &source, std::vector<std::string> *notes)
        {
            Image sized = resized(squared(source, notes), s_nintendo3dsIconSize, s_nintendo3dsIconSize, notes);
            RenderedIcon icon;
            icon.m_preview = flattenedOntoBlack(sized, "the 3DS stores its icon without transparency", notes);
            icon.m_files = {{s_nintendo3dsPath, encodePng(icon.m_preview)}};
            return icon;
        }

        // A 16-color palette whose first entry is transparent, which is how the DS menu reads an icon. ndstool takes a BMP of more colors without a warning and garbles it, so the colors are cut here.
        RenderedIcon forNintendoDs(const Image &source, std::vector<std::string> *notes)
        {
            Image sized = resized(squared(source, notes), s_nintendoDsIconSize, s_nintendoDsIconSize, notes);

            bool isPartlyTransparent = false;
            for (std::size_t offset = 3; offset < sized.m_rgba.size(); offset += 4)
            {
                isPartlyTransparent = isPartlyTransparent || (sized.m_rgba[offset] > 0 && sized.m_rgba[offset] < 255);
            }
            if (isPartlyTransparent)
            {
                notes->push_back("Made transparent where less than half opaque and opaque elsewhere, since the DS icon has no partial transparency.");
            }

            bool wasApproximated = false;
            IndexedImage indexed = quantized(sized, s_nintendoDsColors, true, &wasApproximated);
            if (wasApproximated)
            {
                notes->push_back("Approximated to " + std::to_string(s_nintendoDsColors - 1) +
                                 " colors, which is all the DS icon holds besides transparency.");
            }

            RenderedIcon icon;
            icon.m_files = {{s_nintendoDsPath, encodeSixteenColorBmp(indexed)}};
            icon.m_preview = indexed.toImage();
            icon.m_colorCount = static_cast<int>(indexed.m_palette.size());
            return icon;
        }

        // How much of a pixel a rectangle with round corners covers, sampled at sixteen points.
        double roundedCoverage(int x, int y, int width, int height, double radius)
        {
            constexpr int samples = 4;
            int inside = 0;
            for (int sampleY = 0; sampleY < samples; ++sampleY)
            {
                for (int sampleX = 0; sampleX < samples; ++sampleX)
                {
                    double pointX = x + (sampleX + 0.5) / samples;
                    double pointY = y + (sampleY + 0.5) / samples;
                    double cornerX = std::clamp(pointX, radius, width - radius);
                    double cornerY = std::clamp(pointY, radius, height - radius);
                    double distanceX = pointX - cornerX;
                    double distanceY = pointY - cornerY;
                    inside += distanceX * distanceX + distanceY * distanceY <= radius * radius ? 1 : 0;
                }
            }
            return static_cast<double>(inside) / (samples * samples);
        }

        Image cg50Framed(const Image &art, Color frame)
        {
            Image framed;
            framed.m_width = s_cg50IconWidth;
            framed.m_height = s_cg50IconHeight;
            framed.m_rgba.resize(static_cast<std::size_t>(framed.m_width) * framed.m_height * 4);
            for (int y = 0; y < framed.m_height; ++y)
            {
                for (int x = 0; x < framed.m_width; ++x)
                {
                    double coverage = roundedCoverage(x, y, framed.m_width, framed.m_height, s_cg50FrameRadius);
                    unsigned char *pixel = framed.pixel(x, y);
                    for (int channel = 0; channel < 3; ++channel)
                    {
                        pixel[channel] = static_cast<unsigned char>(std::lround(frame[channel] * coverage + s_white[channel] * (1 - coverage)));
                    }
                    pixel[3] = 255;
                }
            }

            for (int y = 0; y < art.m_height; ++y)
            {
                const unsigned char *row = art.pixel(0, y);
                std::copy(row, row + static_cast<std::size_t>(art.m_width) * 4, framed.pixel(s_cg50FrameWidth, s_cg50FrameWidth + y));
            }
            return framed;
        }

        // The menu draws its icons opaque, on white, so the art is flattened onto white rather than the black the consoles get. The selected icon is the same art with its frame turned blue.
        RenderedIcon forCg50(const Image &source, std::vector<std::string> *notes)
        {
            int artWidth = s_cg50IconWidth - s_cg50FrameWidth * 2;
            int artHeight = s_cg50IconHeight - s_cg50FrameWidth * 2;
            Image art = resized(cropped(source, artWidth, artHeight, notes), artWidth, artHeight, notes);
            if (art.m_hasAlpha)
            {
                notes->push_back("Flattened onto white, since the calculator's menu draws icons without transparency.");
            }
            art = flattened(art, s_white);

            RenderedIcon icon;
            icon.m_preview = cg50Framed(art, s_white);
            icon.m_files = {{s_cg50Path, encodePng(icon.m_preview)}, {s_cg50SelectedPath, encodePng(cg50Framed(art, s_cg50SelectedFrame))}};
            return icon;
        }

        std::string describeOne(const RenderedIcon &icon)
        {
            std::string path = icon.m_files.front().m_path;
            std::string size = std::to_string(icon.m_preview.m_width) + "x" + std::to_string(icon.m_preview.m_height);
            switch (icon.m_platform)
            {
            case Platform::Vita:
                return path + " a " + size + " palette PNG of " + std::to_string(icon.m_colorCount) + " colors";
            case Platform::Switch:
                return path + " a " + size + " JPEG";
            case Platform::Nintendo3ds:
                return path + " a " + size + " PNG";
            case Platform::NintendoDs:
                return path + " a " + size + " BMP of " + std::to_string(icon.m_colorCount) + " colors";
            case Platform::Cg50:
                return path + " a " + size + " PNG, with " + s_cg50SelectedPath + " its selected twin";
            case Platform::Android:
                return path + " a " + size + " PNG";
            default:
                return path + " " + size + " truecolor";
            }
        }
    }

    std::vector<std::string> Icon::paths(Platform platform)
    {
        switch (platform)
        {
        case Platform::Vita:
            return {s_vitaPath};
        case Platform::Switch:
            return {s_switchPath};
        case Platform::Nintendo3ds:
            return {s_nintendo3dsPath};
        case Platform::NintendoDs:
            return {s_nintendoDsPath};
        case Platform::Cg50:
            return {s_cg50Path, s_cg50SelectedPath};
        case Platform::Android:
            return {s_androidPath};
        case Platform::Web:
            return {};
        default:
            return {s_pcPath};
        }
    }

    RenderedIcon Icon::render(Platform platform, const Image &source, std::vector<std::string> *notes)
    {
        RenderedIcon icon;
        if (!source.isNull())
        {
            switch (platform)
            {
            case Platform::Vita:
                icon = forVita(source, notes);
                break;
            case Platform::Switch:
                icon = forSwitch(source, notes);
                break;
            case Platform::Nintendo3ds:
                icon = forNintendo3ds(source, notes);
                break;
            case Platform::NintendoDs:
                icon = forNintendoDs(source, notes);
                break;
            case Platform::Cg50:
                icon = forCg50(source, notes);
                break;
            case Platform::Android:
                icon = forAndroid(source, notes);
                break;
            case Platform::Web:
                break;
            default:
                icon = forPc(source, notes);
            }
        }
        icon.m_platform = platform;
        return icon;
    }

    bool Icon::write(const std::string &projectDirectory, const std::vector<RenderedIcon> &icons, std::string *error)
    {
        std::vector<EncodedFile> files;
        for (const RenderedIcon &icon : icons)
        {
            files.insert(files.end(), icon.m_files.begin(), icon.m_files.end());
        }
        return writeFiles(projectDirectory, files, error);
    }

    std::string Icon::describe(const std::vector<RenderedIcon> &icons)
    {
        std::vector<std::string> written;
        for (const RenderedIcon &icon : icons)
        {
            if (!icon.isNull())
            {
                written.push_back(describeOne(icon));
            }
        }
        if (written.empty())
        {
            return "Nothing was written.";
        }

        // Only the first clause carries the verb, so a single file still reads as a sentence.
        written.front().insert(written.front().find(' '), " is now");
        std::string sentence;
        for (const std::string &clause : written)
        {
            sentence += (sentence.empty() ? "" : ", and ") + clause;
        }
        return sentence + ".";
    }
}
