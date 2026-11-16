#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fried::core
{
    // Truecolor pixels, four bytes each: red, green, blue and alpha, not premultiplied.
    struct Image
    {
        int m_width = 0;
        int m_height = 0;
        std::vector<unsigned char> m_rgba;

        // Whether the file it came from could hold transparency at all, which is what decides if flattening it is worth a note.
        bool m_hasAlpha = false;

        bool isNull() const { return m_width <= 0 || m_height <= 0; }
        const unsigned char *pixel(int x, int y) const { return m_rgba.data() + (static_cast<std::size_t>(y) * m_width + x) * 4; }
        unsigned char *pixel(int x, int y) { return m_rgba.data() + (static_cast<std::size_t>(y) * m_width + x) * 4; }
    };

    using Color = std::array<unsigned char, 4>;

    // Pixels that each name one color of a short list, which is what a console's menu reads.
    struct IndexedImage
    {
        int m_width = 0;
        int m_height = 0;
        std::vector<Color> m_palette;
        std::vector<unsigned char> m_indices;

        Image toImage() const;
    };

    // Reads only the header, so it is cheap. False when the file is not a PNG, JPEG or BMP.
    bool imageSize(const std::string &file, int *width, int *height);

    // Null when the file cannot be read as an image, with the reason in error.
    Image readImage(const std::string &file, std::string *error);
    Image decodeImage(const std::string &bytes);

    // Keeps the middle of the source, cut to the shape of a size. Each step says in notes whatever it had to do to the source.
    Image cropped(const Image &image, int shapeWidth, int shapeHeight, std::vector<std::string> *notes);

    // A console reads one fixed size, so the source is always resized to it.
    Image resized(const Image &image, int width, int height, std::vector<std::string> *notes);
    Image scaled(const Image &image, int width, int height);

    // Composited rather than converted, so a transparent pixel becomes the background instead of whatever color happened to sit underneath it.
    Image flattened(const Image &image, Color background);

    // The image in at most that many colors. With a transparent first color, a pixel less than half opaque takes it and every other becomes opaque. When the image already has few enough colors each keeps its own, and wasApproximated says when it did not.
    IndexedImage quantized(const Image &image, int colorCount, bool hasTransparentFirstColor, bool *wasApproximated);

    std::string encodePng(const Image &image);
    std::string encodeJpeg(const Image &image);
    // One byte a pixel, with the alpha of each color written beside the palette when any is not opaque.
    std::string encodePalettePng(const IndexedImage &image);
    // Four bits a pixel, so the palette holds sixteen colors at most.
    std::string encodeSixteenColorBmp(const IndexedImage &image);

    // Writes a PNG, or a JPEG when the name says so.
    bool writeImage(const std::string &file, const Image &image);

    // Scales an image down, keeping its proportions, until neither side is longer than maxSide, and writes it to the destination.
    bool shrinkImage(const std::string &source, const std::string &destination, int maxSide, std::string *error);

    struct EncodedFile
    {
        // Relative to the directory it is written in.
        std::string m_path;
        std::string m_bytes;
    };

    // Every file is written beside its place before any of them is moved into it, so a failure replaces none.
    bool writeFiles(const std::string &directory, const std::vector<EncodedFile> &files, std::string *error);
}
