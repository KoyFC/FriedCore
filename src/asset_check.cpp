#include "fried/core/asset_check.h"

#include "paths.h"
#include "fried/core/image.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fried::core
{
    namespace fs = std::filesystem;

    namespace
    {
        enum class Kind
        {
            Image,
            Audio,
            Other
        };

        // A file of the project's assets folder, by its path relative to it.
        struct Entry
        {
            std::string m_path;
            Kind m_kind = Kind::Other;

            // "game/sprite.png", the way a scene file names it.
            std::string reference() const { return "game/" + m_path; }
        };

        Kind kindOf(const std::string &path)
        {
            std::string lower = path;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            auto endsWith = [&](const std::string &tail) { return lower.size() >= tail.size() && lower.compare(lower.size() - tail.size(), tail.size(), tail) == 0; };
            if (endsWith(".png") || endsWith(".jpg") || endsWith(".jpeg"))
            {
                return Kind::Image;
            }
            if (endsWith(".wav") || endsWith(".ogg") || endsWith(".mp3"))
            {
                return Kind::Audio;
            }
            return Kind::Other;
        }

        std::vector<Entry> listAssets(const std::string &directory)
        {
            std::vector<Entry> entries;
            std::error_code ignored;
            for (auto entry = fs::recursive_directory_iterator(pathOf(directory), ignored); entry != fs::recursive_directory_iterator(); entry.increment(ignored))
            {
                std::string name = textOf(entry->path().filename());
                if (!entry->is_regular_file(ignored) || name.empty() || name.front() == '.')
                {
                    continue;
                }
                std::string path = textOf(fs::relative(entry->path(), pathOf(directory), ignored));
                entries.push_back({path, kindOf(path)});
            }
            std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.m_path < b.m_path; });
            return entries;
        }

        constexpr int s_maxTextureSide = 1024;
        constexpr std::uintmax_t s_cg50Budget = 2 * 1024 * 1024;

        struct Wav
        {
            bool m_isValid = false;
            int m_format = 0;
            int m_channels = 0;
            int m_rate = 0;
            int m_bits = 0;
            std::vector<unsigned char> m_data;
        };

        std::uint32_t read32(const unsigned char *bytes)
        {
            return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | ((std::uint32_t)bytes[3] << 24);
        }

        std::uint16_t read16(const unsigned char *bytes)
        {
            return (std::uint16_t)(bytes[0] | (bytes[1] << 8));
        }

        Wav readWav(const std::string &file)
        {
            Wav wav;
            std::ifstream stream(pathOf(file), std::ios::binary);
            std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
            if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
            {
                return wav;
            }
            bool hasFormat = false;
            for (std::size_t position = 12; position + 8 <= bytes.size();)
            {
                std::size_t size = read32(bytes.data() + position + 4);
                const unsigned char *chunk = bytes.data() + position + 8;
                size = std::min(size, bytes.size() - position - 8);
                if (std::memcmp(bytes.data() + position, "fmt ", 4) == 0 && size >= 16)
                {
                    wav.m_format = read16(chunk);
                    wav.m_channels = read16(chunk + 2);
                    wav.m_rate = (int)read32(chunk + 4);
                    wav.m_bits = read16(chunk + 14);
                    hasFormat = true;
                }
                else if (std::memcmp(bytes.data() + position, "data", 4) == 0)
                {
                    wav.m_data.assign(chunk, chunk + size);
                    wav.m_isValid = hasFormat;
                    break;
                }
                position += 8 + size + (size & 1);
            }
            return wav;
        }

        void put32(std::vector<unsigned char> &out, std::uint32_t value)
        {
            for (int shift = 0; shift < 32; shift += 8)
            {
                out.push_back((unsigned char)(value >> shift));
            }
        }

        void put16(std::vector<unsigned char> &out, std::uint16_t value)
        {
            out.push_back((unsigned char)value);
            out.push_back((unsigned char)(value >> 8));
        }

        bool writeMonoWav(const std::string &file, const Wav &wav)
        {
            int bytesPerSample = wav.m_bits / 8;
            std::size_t frames = wav.m_data.size() / (bytesPerSample * 2);
            std::vector<unsigned char> mono;
            for (std::size_t frame = 0; frame < frames; ++frame)
            {
                const unsigned char *left = wav.m_data.data() + frame * bytesPerSample * 2;
                const unsigned char *right = left + bytesPerSample;
                if (bytesPerSample == 1)
                {
                    mono.push_back((unsigned char)((left[0] + right[0]) / 2));
                }
                else
                {
                    int sample = ((std::int16_t)read16(left) + (std::int16_t)read16(right)) / 2;
                    put16(mono, (std::uint16_t)(std::int16_t)sample);
                }
            }

            std::vector<unsigned char> out = {'R', 'I', 'F', 'F'};
            put32(out, (std::uint32_t)(36 + mono.size()));
            out.insert(out.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
            put32(out, 16);
            put16(out, 1);
            put16(out, 1);
            put32(out, (std::uint32_t)wav.m_rate);
            put32(out, (std::uint32_t)(wav.m_rate * bytesPerSample));
            put16(out, (std::uint16_t)bytesPerSample);
            put16(out, (std::uint16_t)wav.m_bits);
            out.insert(out.end(), {'d', 'a', 't', 'a'});
            put32(out, (std::uint32_t)mono.size());
            out.insert(out.end(), mono.begin(), mono.end());

            std::ofstream stream(pathOf(file), std::ios::binary | std::ios::trunc);
            stream.write((const char *)out.data(), (std::streamsize)out.size());
            return (bool)stream;
        }

        void check(Platform platform, const Entry &entry, const std::string &file, std::vector<AssetIssue> &issues)
        {
            bool isConsole3d = platform == Platform::Nintendo3ds || platform == Platform::NintendoDs;
            if (entry.m_kind == Kind::Image && isConsole3d)
            {
                int width = 0, height = 0;
                if (imageSize(file, &width, &height) && (width > s_maxTextureSide || height > s_maxTextureSide))
                {
                    issues.push_back({AssetIssue::Severity::Error, entry.reference(),
                                      "The " + platformLabel(platform) + " cannot hold a texture over " + std::to_string(s_maxTextureSide) + " pixels a side, and this is "
                                          + std::to_string(width) + "x" + std::to_string(height) + ".",
                                      AssetIssue::Fix::ShrinkImage, "Shrink to " + std::to_string(s_maxTextureSide) + " pixels"});
                }
                else if (platform == Platform::NintendoDs && width * height * 2 > 256 * 1024)
                {
                    issues.push_back({AssetIssue::Severity::Warning, entry.reference(),
                                      "At " + std::to_string(width) + "x" + std::to_string(height) + " this texture alone is over the DS's 256 KB of texture memory.", AssetIssue::Fix::None, ""});
                }
            }

            if (entry.m_kind == Kind::Audio)
            {
                if (platform == Platform::Cg50)
                {
                    issues.push_back({AssetIssue::Severity::Warning, entry.reference(), "The fx-CG50 has no audio, so this file only takes up space.", AssetIssue::Fix::None, ""});
                }
                else if (platform == Platform::NintendoDs)
                {
                    Wav wav = readWav(file);
                    if (!wav.m_isValid)
                    {
                        issues.push_back({AssetIssue::Severity::Error, entry.reference(), "The DS plays PCM WAV only, and this is not one.", AssetIssue::Fix::None, ""});
                    }
                    else if (wav.m_format != 1 || (wav.m_bits != 8 && wav.m_bits != 16))
                    {
                        issues.push_back({AssetIssue::Severity::Error, entry.reference(), "The DS plays uncompressed 8 or 16 bit WAV, and this is not.", AssetIssue::Fix::None, ""});
                    }
                    else if (wav.m_channels == 2)
                    {
                        issues.push_back({AssetIssue::Severity::Error, entry.reference(), "The DS plays mono WAV, and this is stereo.", AssetIssue::Fix::DownmixWav, "Convert to mono"});
                    }
                    else if (wav.m_channels != 1)
                    {
                        issues.push_back({AssetIssue::Severity::Error, entry.reference(), "The DS plays mono WAV only.", AssetIssue::Fix::None, ""});
                    }
                }
            }
        }

        // A PNG names its color type in the header, and 3 is a palette.
        bool isPalettePng(const std::string &file)
        {
            const unsigned char signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
            unsigned char header[26] = {};
            std::ifstream stream(pathOf(file), std::ios::binary);
            stream.read((char *)header, sizeof(header));
            return stream && std::memcmp(header, signature, sizeof(signature)) == 0 && std::memcmp(header + 12, "IHDR", 4) == 0 && header[25] == 3;
        }

        const std::string s_vitaPackageDirectory = "sce_sys";

        void checkVitaPackage(const std::string &projectDirectory, std::vector<AssetIssue> &issues)
        {
            for (const Entry &entry : listAssets(projectDirectory + "/" + s_vitaPackageDirectory))
            {
                std::string path = s_vitaPackageDirectory + "/" + entry.m_path;
                if (entry.m_kind == Kind::Image && !isPalettePng(projectDirectory + "/" + path))
                {
                    issues.push_back({AssetIssue::Severity::Error, path, "The Vita's installer refuses an image that is not a palette PNG, and this is not one.",
                                      AssetIssue::Fix::PaletteImage, "Convert to 256 colors"});
                }
            }
        }

        constexpr int s_paletteColorLimit = 256;

        bool writePaletteImage(const std::string &source, const std::string &destination, std::string *error)
        {
            Image image = readImage(source, error);
            if (image.isNull())
            {
                return false;
            }
            bool wasApproximated = false;
            return writeFiles(textOf(pathOf(destination).parent_path()), {{textOf(pathOf(destination).filename()), encodePalettePng(quantized(image, s_paletteColorLimit, false, &wasApproximated))}}, error);
        }
    }

    std::vector<AssetIssue> validateAssets(Platform platform, const std::string &projectDirectory)
    {
        std::vector<AssetIssue> issues;
        std::string assetsDirectory = projectDirectory + "/assets";
        std::uintmax_t total = 0;
        for (const Entry &entry : listAssets(assetsDirectory))
        {
            std::string file = assetsDirectory + "/" + entry.m_path;
            std::error_code ignored;
            total += fs::file_size(pathOf(file), ignored);
            check(platform, entry, file, issues);
        }
        if (platform == Platform::Cg50 && total > s_cg50Budget)
        {
            issues.push_back({AssetIssue::Severity::Error, "",
                              "The assets add up to " + std::to_string(total / 1024) + " KB, and the fx-CG50 has room for 2048 KB beside the game.", AssetIssue::Fix::None, ""});
        }
        if (platform == Platform::Vita)
        {
            checkVitaPackage(projectDirectory, issues);
        }
        return issues;
    }

    bool applyFix(const AssetIssue &issue, const std::string &projectDirectory, const std::string &backupDirectory, std::string *error)
    {
        const std::string assetPrefix = "game/";
        const std::string packagePrefix = s_vitaPackageDirectory + "/";
        bool isAsset = issue.m_asset.rfind(assetPrefix, 0) == 0 && issue.m_asset.size() > assetPrefix.size();
        bool isPackageFile = issue.m_asset.rfind(packagePrefix, 0) == 0 && issue.m_asset.size() > packagePrefix.size();
        if (issue.m_fix == AssetIssue::Fix::None || (!isAsset && !isPackageFile))
        {
            *error = "This issue has no fix.";
            return false;
        }

        // An asset is kept by its path in the assets folder and a package file by its path in the project, which is where each is found.
        std::string path = isAsset ? issue.m_asset.substr(assetPrefix.size()) : issue.m_asset;
        std::string file = (isAsset ? projectDirectory + "/assets" : projectDirectory) + "/" + path;
        fs::path backup = pathOf(backupDirectory) / pathOf(path);
        std::error_code failure;
        fs::create_directories(backup.parent_path(), failure);
        if (!fs::exists(backup))
        {
            fs::copy_file(pathOf(file), backup, failure);
            if (failure)
            {
                *error = "Cannot keep a copy of " + path + ": " + failure.message();
                return false;
            }
        }

        if (issue.m_fix == AssetIssue::Fix::ShrinkImage)
        {
            return shrinkImage(textOf(backup), file, s_maxTextureSide, error);
        }
        if (issue.m_fix == AssetIssue::Fix::PaletteImage)
        {
            return writePaletteImage(textOf(backup), file, error);
        }

        Wav wav = readWav(textOf(backup));
        if (!wav.m_isValid || wav.m_channels != 2 || !writeMonoWav(file, wav))
        {
            *error = "Cannot convert " + path + " to mono.";
            return false;
        }
        return true;
    }
}
