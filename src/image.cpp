#include "fried/core/image.h"

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

// Defined by stb_image_write and left out of its header.
extern "C" unsigned char *stbi_zlib_compress(unsigned char *data, int data_len, int *out_len, int quality);

namespace fried::core
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr int s_jpegQuality = 92;
        constexpr int s_compression = 8;

        bool endsWith(const std::string &text, const std::string &suffix)
        {
            return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        bool isJpeg(const std::string &file)
        {
            std::string lower = file;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return endsWith(lower, ".jpg") || endsWith(lower, ".jpeg");
        }

        std::string sizeText(int width, int height)
        {
            return std::to_string(width) + "x" + std::to_string(height);
        }

        void appendBytes(void *context, void *data, int size)
        {
            static_cast<std::string *>(context)->append(static_cast<const char *>(data), static_cast<std::size_t>(size));
        }

        Image fromStb(unsigned char *pixels, int width, int height, int channels)
        {
            Image image;
            if (pixels == nullptr)
            {
                return image;
            }
            image.m_width = width;
            image.m_height = height;
            image.m_hasAlpha = channels == 2 || channels == 4;
            image.m_rgba.assign(pixels, pixels + static_cast<std::size_t>(width) * height * 4);
            stbi_image_free(pixels);
            return image;
        }

        // Each new pixel is the average of the area of the old image it covers, weighted by alpha so transparent edges do not bleed their color.
        void averageArea(const Image &source, double left, double top, double right, double bottom, unsigned char *out)
        {
            double red = 0, green = 0, blue = 0, alpha = 0, area = 0;
            int firstY = static_cast<int>(std::floor(top));
            int lastY = std::min(source.m_height - 1, static_cast<int>(std::ceil(bottom)) - 1);
            int firstX = static_cast<int>(std::floor(left));
            int lastX = std::min(source.m_width - 1, static_cast<int>(std::ceil(right)) - 1);
            for (int y = firstY; y <= lastY; ++y)
            {
                double rowWeight = std::min<double>(bottom, y + 1) - std::max<double>(top, y);
                for (int x = firstX; x <= lastX; ++x)
                {
                    double weight = rowWeight * (std::min<double>(right, x + 1) - std::max<double>(left, x));
                    const unsigned char *pixel = source.pixel(x, y);
                    red += weight * pixel[0] * pixel[3];
                    green += weight * pixel[1] * pixel[3];
                    blue += weight * pixel[2] * pixel[3];
                    alpha += weight * pixel[3];
                    area += weight;
                }
            }
            out[0] = alpha > 0 ? static_cast<unsigned char>(std::lround(red / alpha)) : 0;
            out[1] = alpha > 0 ? static_cast<unsigned char>(std::lround(green / alpha)) : 0;
            out[2] = alpha > 0 ? static_cast<unsigned char>(std::lround(blue / alpha)) : 0;
            out[3] = area > 0 ? static_cast<unsigned char>(std::lround(alpha / area)) : 0;
        }

        // Between the four old pixels around the new one's center, again weighted by alpha.
        void interpolate(const Image &source, double x, double y, unsigned char *out)
        {
            double clampedX = std::clamp(x - 0.5, 0.0, source.m_width - 1.0);
            double clampedY = std::clamp(y - 0.5, 0.0, source.m_height - 1.0);
            int left = static_cast<int>(clampedX);
            int top = static_cast<int>(clampedY);
            int right = std::min(left + 1, source.m_width - 1);
            int bottom = std::min(top + 1, source.m_height - 1);
            double alongX = clampedX - left;
            double alongY = clampedY - top;

            double red = 0, green = 0, blue = 0, alpha = 0;
            auto add = [&](int pixelX, int pixelY, double weight) {
                const unsigned char *pixel = source.pixel(pixelX, pixelY);
                red += weight * pixel[0] * pixel[3];
                green += weight * pixel[1] * pixel[3];
                blue += weight * pixel[2] * pixel[3];
                alpha += weight * pixel[3];
            };
            add(left, top, (1 - alongX) * (1 - alongY));
            add(right, top, alongX * (1 - alongY));
            add(left, bottom, (1 - alongX) * alongY);
            add(right, bottom, alongX * alongY);

            out[0] = alpha > 0 ? static_cast<unsigned char>(std::lround(red / alpha)) : 0;
            out[1] = alpha > 0 ? static_cast<unsigned char>(std::lround(green / alpha)) : 0;
            out[2] = alpha > 0 ? static_cast<unsigned char>(std::lround(blue / alpha)) : 0;
            out[3] = static_cast<unsigned char>(std::lround(alpha));
        }

        std::uint32_t packed(const unsigned char *pixel)
        {
            return (static_cast<std::uint32_t>(pixel[0]) << 24) | (static_cast<std::uint32_t>(pixel[1]) << 16) |
                   (static_cast<std::uint32_t>(pixel[2]) << 8) | pixel[3];
        }

        Color unpacked(std::uint32_t color)
        {
            return {static_cast<unsigned char>(color >> 24), static_cast<unsigned char>(color >> 16), static_cast<unsigned char>(color >> 8),
                    static_cast<unsigned char>(color)};
        }

        int channelOf(std::uint32_t color, int index)
        {
            return static_cast<int>((color >> (24 - index * 8)) & 0xff);
        }

        // Median cut: the box of colors spread widest along one channel is split at its median until there are enough boxes, and each box becomes its average.
        std::vector<Color> fewColors(std::vector<std::uint32_t> colors, int count)
        {
            struct Box
            {
                std::size_t m_begin;
                std::size_t m_end;
            };

            auto widest = [&colors](const Box &box, int *range) {
                int best = 0;
                *range = -1;
                for (int index = 0; index < 4; ++index)
                {
                    int low = 255, high = 0;
                    for (std::size_t color = box.m_begin; color < box.m_end; ++color)
                    {
                        low = std::min(low, channelOf(colors[color], index));
                        high = std::max(high, channelOf(colors[color], index));
                    }
                    if (high - low > *range)
                    {
                        *range = high - low;
                        best = index;
                    }
                }
                return best;
            };

            std::vector<Box> boxes = {{0, colors.size()}};
            while (static_cast<int>(boxes.size()) < count)
            {
                int chosen = -1, chosenRange = 0, chosenChannel = 0;
                for (int index = 0; index < static_cast<int>(boxes.size()); ++index)
                {
                    int range = 0;
                    int along = widest(boxes[index], &range);
                    if (range > chosenRange)
                    {
                        chosen = index;
                        chosenRange = range;
                        chosenChannel = along;
                    }
                }
                if (chosen < 0)
                {
                    break;
                }

                Box box = boxes[chosen];
                std::size_t middle = box.m_begin + (box.m_end - box.m_begin) / 2;
                std::nth_element(colors.begin() + box.m_begin, colors.begin() + middle, colors.begin() + box.m_end,
                                 [chosenChannel](std::uint32_t a, std::uint32_t b) { return channelOf(a, chosenChannel) < channelOf(b, chosenChannel); });
                boxes[chosen] = {box.m_begin, middle};
                boxes.push_back({middle, box.m_end});
            }

            std::vector<Color> palette;
            for (const Box &box : boxes)
            {
                long sums[4] = {0, 0, 0, 0};
                for (std::size_t color = box.m_begin; color < box.m_end; ++color)
                {
                    for (int index = 0; index < 4; ++index)
                    {
                        sums[index] += channelOf(colors[color], index);
                    }
                }
                long size = static_cast<long>(box.m_end - box.m_begin);
                palette.push_back({static_cast<unsigned char>(sums[0] / size), static_cast<unsigned char>(sums[1] / size),
                                   static_cast<unsigned char>(sums[2] / size), static_cast<unsigned char>(sums[3] / size)});
            }
            return palette;
        }

        int nearestOf(const std::vector<Color> &palette, int first, const unsigned char *pixel)
        {
            int nearest = first, nearestDistance = INT_MAX;
            for (int index = first; index < static_cast<int>(palette.size()); ++index)
            {
                int distance = 0;
                for (int channel = 0; channel < 4; ++channel)
                {
                    int difference = palette[index][channel] - pixel[channel];
                    distance += difference * difference;
                }
                if (distance < nearestDistance)
                {
                    nearest = index;
                    nearestDistance = distance;
                }
            }
            return nearest;
        }

        void appendBigEndian(std::string &bytes, std::uint32_t value)
        {
            for (int shift = 24; shift >= 0; shift -= 8)
            {
                bytes.push_back(static_cast<char>((value >> shift) & 0xff));
            }
        }

        void appendLittleEndian(std::string &bytes, std::uint32_t value, int count)
        {
            for (int index = 0; index < count; ++index)
            {
                bytes.push_back(static_cast<char>((value >> (index * 8)) & 0xff));
            }
        }

        std::uint32_t checksum(const std::string &bytes)
        {
            static std::uint32_t s_table[256];
            static bool s_isBuilt = false;
            if (!s_isBuilt)
            {
                for (std::uint32_t index = 0; index < 256; ++index)
                {
                    std::uint32_t value = index;
                    for (int bit = 0; bit < 8; ++bit)
                    {
                        value = (value & 1) ? 0xedb88320u ^ (value >> 1) : value >> 1;
                    }
                    s_table[index] = value;
                }
                s_isBuilt = true;
            }

            std::uint32_t crc = 0xffffffffu;
            for (unsigned char byte : bytes)
            {
                crc = s_table[(crc ^ byte) & 0xff] ^ (crc >> 8);
            }
            return crc ^ 0xffffffffu;
        }

        void appendChunk(std::string &png, const char *type, const std::string &data)
        {
            appendBigEndian(png, static_cast<std::uint32_t>(data.size()));
            std::string body = std::string(type, 4) + data;
            png += body;
            appendBigEndian(png, checksum(body));
        }
    }

    Image IndexedImage::toImage() const
    {
        Image image;
        image.m_width = m_width;
        image.m_height = m_height;
        image.m_rgba.resize(m_indices.size() * 4);
        for (std::size_t index = 0; index < m_indices.size(); ++index)
        {
            const Color &color = m_palette[m_indices[index]];
            std::copy(color.begin(), color.end(), image.m_rgba.begin() + index * 4);
            image.m_hasAlpha = image.m_hasAlpha || color[3] < 255;
        }
        return image;
    }

    bool imageSize(const std::string &file, int *width, int *height)
    {
        int channels = 0;
        return stbi_info(file.c_str(), width, height, &channels) != 0;
    }

    Image readImage(const std::string &file, std::string *error)
    {
        int width = 0, height = 0, channels = 0;
        unsigned char *pixels = stbi_load(file.c_str(), &width, &height, &channels, 4);
        Image image = fromStb(pixels, width, height, channels);
        if (image.isNull())
        {
            const char *reason = stbi_failure_reason();
            *error = fs::path(file).filename().string() + " could not be read as an image: " + (reason != nullptr ? reason : "unknown format") + ".";
        }
        return image;
    }

    Image decodeImage(const std::string &bytes)
    {
        int width = 0, height = 0, channels = 0;
        unsigned char *pixels = stbi_load_from_memory(reinterpret_cast<const unsigned char *>(bytes.data()), static_cast<int>(bytes.size()), &width,
                                                      &height, &channels, 4);
        return fromStb(pixels, width, height, channels);
    }

    Image cropped(const Image &image, int shapeWidth, int shapeHeight, std::vector<std::string> *notes)
    {
        // Compared crosswise so no rounding decides whether the shapes match.
        long long imageCross = static_cast<long long>(image.m_width) * shapeHeight;
        long long shapeCross = static_cast<long long>(shapeWidth) * image.m_height;
        if (imageCross == shapeCross)
        {
            return image;
        }

        int keptWidth = image.m_width;
        int keptHeight = image.m_height;
        if (imageCross > shapeCross)
        {
            keptWidth = static_cast<int>(static_cast<long long>(image.m_height) * shapeWidth / shapeHeight);
        }
        else
        {
            keptHeight = static_cast<int>(static_cast<long long>(image.m_width) * shapeHeight / shapeWidth);
        }
        keptWidth = std::max(1, keptWidth);
        keptHeight = std::max(1, keptHeight);

        notes->push_back("Uses the center " + sizeText(keptWidth, keptHeight) + " of " + sizeText(image.m_width, image.m_height) + ".");

        Image kept;
        kept.m_width = keptWidth;
        kept.m_height = keptHeight;
        kept.m_hasAlpha = image.m_hasAlpha;
        kept.m_rgba.resize(static_cast<std::size_t>(keptWidth) * keptHeight * 4);
        int left = (image.m_width - keptWidth) / 2;
        int top = (image.m_height - keptHeight) / 2;
        for (int y = 0; y < keptHeight; ++y)
        {
            const unsigned char *row = image.pixel(left, top + y);
            std::copy(row, row + static_cast<std::size_t>(keptWidth) * 4, kept.pixel(0, y));
        }
        return kept;
    }

    Image scaled(const Image &image, int width, int height)
    {
        if (image.isNull() || (image.m_width == width && image.m_height == height))
        {
            return image;
        }

        Image result;
        result.m_width = width;
        result.m_height = height;
        result.m_hasAlpha = image.m_hasAlpha;
        result.m_rgba.resize(static_cast<std::size_t>(width) * height * 4);

        double stepX = static_cast<double>(image.m_width) / width;
        double stepY = static_cast<double>(image.m_height) / height;
        bool isReduction = stepX >= 1.0 && stepY >= 1.0;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                if (isReduction)
                {
                    averageArea(image, x * stepX, y * stepY, (x + 1) * stepX, (y + 1) * stepY, result.pixel(x, y));
                }
                else
                {
                    interpolate(image, (x + 0.5) * stepX, (y + 0.5) * stepY, result.pixel(x, y));
                }
            }
        }
        return result;
    }

    Image resized(const Image &image, int width, int height, std::vector<std::string> *notes)
    {
        if (image.m_width < width)
        {
            notes->push_back("Enlarged from " + sizeText(image.m_width, image.m_height) + ", so it will look soft.");
        }
        else if (image.m_width > width)
        {
            notes->push_back("Reduced from " + sizeText(image.m_width, image.m_height) + ".");
        }
        return scaled(image, width, height);
    }

    Image flattened(const Image &image, Color background)
    {
        Image result = image;
        result.m_hasAlpha = false;
        for (std::size_t offset = 0; offset < result.m_rgba.size(); offset += 4)
        {
            unsigned char *pixel = result.m_rgba.data() + offset;
            int alpha = pixel[3];
            for (int channel = 0; channel < 3; ++channel)
            {
                pixel[channel] = static_cast<unsigned char>((pixel[channel] * alpha + background[channel] * (255 - alpha) + 127) / 255);
            }
            pixel[3] = 255;
        }
        return result;
    }

    IndexedImage quantized(const Image &image, int colorCount, bool hasTransparentFirstColor, bool *wasApproximated)
    {
        IndexedImage indexed;
        indexed.m_width = image.m_width;
        indexed.m_height = image.m_height;
        *wasApproximated = false;

        std::size_t pixelCount = static_cast<std::size_t>(image.m_width) * image.m_height;
        std::vector<unsigned char> pixels = image.m_rgba;
        std::vector<std::uint32_t> colors;
        for (std::size_t index = 0; index < pixelCount; ++index)
        {
            unsigned char *pixel = pixels.data() + index * 4;
            if (hasTransparentFirstColor)
            {
                if (pixel[3] < 128)
                {
                    continue;
                }
                pixel[3] = 255;
            }
            colors.push_back(packed(pixel));
        }

        int first = 0;
        if (hasTransparentFirstColor)
        {
            indexed.m_palette.push_back({0, 0, 0, 0});
            first = 1;
        }

        if (!colors.empty())
        {
            std::vector<std::uint32_t> distinct = colors;
            std::sort(distinct.begin(), distinct.end());
            distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());

            int available = colorCount - first;
            if (static_cast<int>(distinct.size()) > available)
            {
                *wasApproximated = true;
                std::vector<Color> cut = fewColors(colors, available);
                indexed.m_palette.insert(indexed.m_palette.end(), cut.begin(), cut.end());
            }
            else
            {
                for (std::uint32_t color : distinct)
                {
                    indexed.m_palette.push_back(unpacked(color));
                }
            }
        }

        indexed.m_indices.resize(pixelCount);
        for (std::size_t index = 0; index < pixelCount; ++index)
        {
            const unsigned char *original = image.m_rgba.data() + index * 4;
            bool isTransparent = hasTransparentFirstColor && original[3] < 128;
            indexed.m_indices[index] = static_cast<unsigned char>(isTransparent ? 0 : nearestOf(indexed.m_palette, first, pixels.data() + index * 4));
        }
        return indexed;
    }

    std::string encodePng(const Image &image)
    {
        std::string bytes;
        if (image.m_hasAlpha)
        {
            stbi_write_png_to_func(appendBytes, &bytes, image.m_width, image.m_height, 4, image.m_rgba.data(), image.m_width * 4);
            return bytes;
        }

        std::vector<unsigned char> opaque(static_cast<std::size_t>(image.m_width) * image.m_height * 3);
        for (std::size_t index = 0; index < opaque.size() / 3; ++index)
        {
            std::copy(image.m_rgba.begin() + index * 4, image.m_rgba.begin() + index * 4 + 3, opaque.begin() + index * 3);
        }
        stbi_write_png_to_func(appendBytes, &bytes, image.m_width, image.m_height, 3, opaque.data(), image.m_width * 3);
        return bytes;
    }

    std::string encodeJpeg(const Image &image)
    {
        std::string bytes;
        stbi_write_jpg_to_func(appendBytes, &bytes, image.m_width, image.m_height, 4, image.m_rgba.data(), s_jpegQuality);
        return bytes;
    }

    std::string encodePalettePng(const IndexedImage &image)
    {
        std::string png("\x89PNG\r\n\x1a\n", 8);

        std::string header;
        appendBigEndian(header, static_cast<std::uint32_t>(image.m_width));
        appendBigEndian(header, static_cast<std::uint32_t>(image.m_height));
        // Eight bits a pixel, palette color, and the only compression, filter and interlace methods there are.
        header += std::string("\x08\x03\x00\x00\x00", 5);
        appendChunk(png, "IHDR", header);

        std::string palette, transparency;
        bool hasTransparency = false;
        for (const Color &color : image.m_palette)
        {
            palette.append(reinterpret_cast<const char *>(color.data()), 3);
            transparency.push_back(static_cast<char>(color[3]));
            hasTransparency = hasTransparency || color[3] < 255;
        }
        appendChunk(png, "PLTE", palette);
        if (hasTransparency)
        {
            appendChunk(png, "tRNS", transparency);
        }

        // Each row starts with the filter it uses, which is none.
        std::string rows;
        for (int y = 0; y < image.m_height; ++y)
        {
            rows.push_back('\0');
            rows.append(reinterpret_cast<const char *>(image.m_indices.data()) + static_cast<std::size_t>(y) * image.m_width,
                        static_cast<std::size_t>(image.m_width));
        }
        int compressedSize = 0;
        unsigned char *compressed =
            stbi_zlib_compress(reinterpret_cast<unsigned char *>(rows.data()), static_cast<int>(rows.size()), &compressedSize, s_compression);
        appendChunk(png, "IDAT", std::string(reinterpret_cast<const char *>(compressed), static_cast<std::size_t>(compressedSize)));
        std::free(compressed);

        appendChunk(png, "IEND", std::string());
        return png;
    }

    std::string encodeSixteenColorBmp(const IndexedImage &image)
    {
        constexpr std::uint32_t headerSize = 14 + 40 + 16 * 4;
        std::uint32_t rowSize = static_cast<std::uint32_t>(((image.m_width * 4 + 31) / 32) * 4);
        std::uint32_t pixelSize = rowSize * static_cast<std::uint32_t>(image.m_height);

        std::string bmp = "BM";
        appendLittleEndian(bmp, headerSize + pixelSize, 4);
        appendLittleEndian(bmp, 0, 4);
        appendLittleEndian(bmp, headerSize, 4);

        appendLittleEndian(bmp, 40, 4);
        appendLittleEndian(bmp, static_cast<std::uint32_t>(image.m_width), 4);
        appendLittleEndian(bmp, static_cast<std::uint32_t>(image.m_height), 4);
        appendLittleEndian(bmp, 1, 2);
        appendLittleEndian(bmp, 4, 2);
        appendLittleEndian(bmp, 0, 4);
        appendLittleEndian(bmp, pixelSize, 4);
        // 72 dots an inch, in dots a meter.
        appendLittleEndian(bmp, 2835, 4);
        appendLittleEndian(bmp, 2835, 4);
        appendLittleEndian(bmp, 16, 4);
        appendLittleEndian(bmp, 0, 4);

        for (std::size_t index = 0; index < 16; ++index)
        {
            Color color = index < image.m_palette.size() ? image.m_palette[index] : Color{0, 0, 0, 255};
            bmp.push_back(static_cast<char>(color[2]));
            bmp.push_back(static_cast<char>(color[1]));
            bmp.push_back(static_cast<char>(color[0]));
            bmp.push_back('\0');
        }

        // Rows go from the bottom up, two pixels to a byte with the left one in the high half.
        for (int y = image.m_height - 1; y >= 0; --y)
        {
            std::string row(rowSize, '\0');
            for (int x = 0; x < image.m_width; ++x)
            {
                unsigned char index = image.m_indices[static_cast<std::size_t>(y) * image.m_width + x] & 0x0f;
                row[x / 2] = static_cast<char>(static_cast<unsigned char>(row[x / 2]) | (x % 2 == 0 ? index << 4 : index));
            }
            bmp += row;
        }
        return bmp;
    }

    bool writeImage(const std::string &file, const Image &image)
    {
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        stream << (isJpeg(file) ? encodeJpeg(image) : encodePng(image));
        stream.flush();
        return static_cast<bool>(stream);
    }

    bool shrinkImage(const std::string &source, const std::string &destination, int maxSide, std::string *error)
    {
        Image image = readImage(source, error);
        if (image.isNull())
        {
            *error = "Cannot read " + source + ".";
            return false;
        }

        double ratio = std::min(1.0, static_cast<double>(maxSide) / std::max(image.m_width, image.m_height));
        int width = std::max(1, static_cast<int>(std::lround(image.m_width * ratio)));
        int height = std::max(1, static_cast<int>(std::lround(image.m_height * ratio)));
        if (!writeImage(destination, scaled(image, width, height)))
        {
            *error = "Cannot write " + destination + ".";
            return false;
        }
        return true;
    }

    bool writeFiles(const std::string &directory, const std::vector<EncodedFile> &files, std::string *error)
    {
        std::vector<std::pair<fs::path, fs::path>> staged;
        auto discard = [&staged]() {
            for (const auto &[temporary, target] : staged)
            {
                std::error_code ignored;
                fs::remove(temporary, ignored);
            }
        };

        for (const EncodedFile &file : files)
        {
            fs::path target = fs::path(directory) / file.m_path;
            fs::path temporary = target;
            temporary += ".tmp";

            std::error_code failure;
            fs::create_directories(target.parent_path(), failure);
            if (failure)
            {
                *error = "Could not create the directory for " + target.string() + ".";
                discard();
                return false;
            }

            staged.emplace_back(temporary, target);
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            stream << file.m_bytes;
            stream.flush();
            if (!stream)
            {
                *error = "Could not write " + target.string() + ".";
                discard();
                return false;
            }
        }

        for (const auto &[temporary, target] : staged)
        {
            std::error_code failure;
            fs::rename(temporary, target, failure);
            if (failure)
            {
                *error = "Could not write " + target.string() + ": " + failure.message();
                discard();
                return false;
            }
        }
        return true;
    }
}
