#include "doctest/doctest.h"

#include "fried/core/icon.h"
#include "temporary_directory.h"

#include <algorithm>
#include <filesystem>
#include <set>

using namespace fried::core;

namespace
{
    // A gradient with a transparent corner and more colors than any palette holds.
    Image gradient(int width, int height)
    {
        Image image;
        image.m_width = width;
        image.m_height = height;
        image.m_hasAlpha = true;
        image.m_rgba.resize(static_cast<std::size_t>(width) * height * 4);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                unsigned char *pixel = image.pixel(x, y);
                pixel[0] = static_cast<unsigned char>(x * 255 / (width - 1));
                pixel[1] = static_cast<unsigned char>(y * 255 / (height - 1));
                pixel[2] = static_cast<unsigned char>((x + y) % 256);
                pixel[3] = x < width / 4 && y < height / 4 ? 0 : 255;
            }
        }
        return image;
    }

    Image flat(int width, int height, Color color)
    {
        Image image;
        image.m_width = width;
        image.m_height = height;
        image.m_rgba.resize(static_cast<std::size_t>(width) * height * 4);
        for (std::size_t offset = 0; offset < image.m_rgba.size(); offset += 4)
        {
            std::copy(color.begin(), color.end(), image.m_rgba.begin() + offset);
        }
        return image;
    }

    std::uint32_t bigEndian(const std::string &bytes, std::size_t at)
    {
        return (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at])) << 24) | (static_cast<unsigned char>(bytes[at + 1]) << 16) |
               (static_cast<unsigned char>(bytes[at + 2]) << 8) | static_cast<unsigned char>(bytes[at + 3]);
    }

    std::uint32_t littleEndian(const std::string &bytes, std::size_t at, int count)
    {
        std::uint32_t value = 0;
        for (int index = 0; index < count; ++index)
        {
            value |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + index])) << (index * 8);
        }
        return value;
    }

    // The names and sizes of a PNG's chunks, in order.
    std::vector<std::pair<std::string, std::uint32_t>> chunksOf(const std::string &png)
    {
        std::vector<std::pair<std::string, std::uint32_t>> chunks;
        for (std::size_t at = 8; at + 8 <= png.size();)
        {
            std::uint32_t size = bigEndian(png, at);
            chunks.emplace_back(png.substr(at + 4, 4), size);
            at += 12 + size;
        }
        return chunks;
    }
}

TEST_CASE("every platform keeps its icon where its build looks for it")
{
    CHECK(Icon::paths(Platform::Pc) == std::vector<std::string>{"assets/icon.png"});
    CHECK(Icon::paths(Platform::Vita) == std::vector<std::string>{"sce_sys/icon0.png"});
    CHECK(Icon::paths(Platform::Switch) == std::vector<std::string>{"switch/icon.jpg"});
    CHECK(Icon::paths(Platform::Nintendo3ds) == std::vector<std::string>{"3ds/icon.png"});
    CHECK(Icon::paths(Platform::NintendoDs) == std::vector<std::string>{"nds/icon.bmp"});
    CHECK(Icon::paths(Platform::Cg50) == std::vector<std::string>{"cg50/icon-uns.png", "cg50/icon-sel.png"});
    CHECK(Icon::paths(Platform::Web).empty());
}

TEST_CASE("a PC icon is square, truecolor and no larger than a window icon needs")
{
    std::vector<std::string> notes;
    RenderedIcon icon = Icon::render(Platform::Pc, gradient(1200, 800), &notes);
    Image written = decodeImage(icon.m_files.front().m_bytes);
    CHECK(written.m_width == 512);
    CHECK(written.m_height == 512);
    CHECK(written.m_hasAlpha);
    CHECK(notes == std::vector<std::string>{"Uses the center 800x800 of 1200x800.", "Reduced from 800x800 to 512x512."});

    notes.clear();
    RenderedIcon small = Icon::render(Platform::Pc, flat(64, 64, {10, 20, 30, 255}), &notes);
    Image kept = decodeImage(small.m_files.front().m_bytes);
    CHECK(kept.m_width == 64);
    CHECK_FALSE(kept.m_hasAlpha);
    CHECK(kept.m_rgba[0] == 10);
    CHECK(notes.empty());
}

TEST_CASE("a Vita icon is a 128x128 palette PNG")
{
    std::vector<std::string> notes;
    RenderedIcon icon = Icon::render(Platform::Vita, gradient(256, 256), &notes);
    const std::string &png = icon.m_files.front().m_bytes;

    CHECK(png.substr(0, 8) == std::string("\x89PNG\r\n\x1a\n", 8));
    auto chunks = chunksOf(png);
    REQUIRE(chunks.size() == 5);
    CHECK(chunks[0].first == "IHDR");
    CHECK(chunks[1].first == "PLTE");
    CHECK(chunks[2].first == "tRNS");
    CHECK(chunks[3].first == "IDAT");
    CHECK(chunks[4].first == "IEND");
    CHECK(bigEndian(png, 16) == 128);
    CHECK(bigEndian(png, 20) == 128);
    // Eight bits a pixel, palette color.
    CHECK(png[24] == 8);
    CHECK(png[25] == 3);
    CHECK(chunks[1].second <= 256 * 3);
    CHECK(chunks[1].second % 3 == 0);
    CHECK(icon.m_colorCount == 256);
    CHECK(notes == std::vector<std::string>{"Reduced from 256x256.", "Approximated to 256 colors, which is all a palette PNG holds."});

    Image decoded = decodeImage(png);
    CHECK(decoded.m_width == 128);
    CHECK(decoded.pixel(0, 0)[3] == 0);
    CHECK(decoded.pixel(127, 127)[3] == 255);
    CHECK(decoded.m_rgba == icon.m_preview.m_rgba);
}

TEST_CASE("an image of few colors keeps each of them in its palette")
{
    std::vector<std::string> notes;
    RenderedIcon icon = Icon::render(Platform::Vita, flat(128, 128, {200, 100, 50, 255}), &notes);
    CHECK(icon.m_colorCount == 1);
    CHECK(notes.empty());
    auto chunks = chunksOf(icon.m_files.front().m_bytes);
    CHECK(chunks.size() == 4);
    Image decoded = decodeImage(icon.m_files.front().m_bytes);
    CHECK(decoded.pixel(64, 64)[0] == 200);
    CHECK(decoded.pixel(64, 64)[1] == 100);
    CHECK(decoded.pixel(64, 64)[2] == 50);
}

TEST_CASE("a Switch icon is a 256x256 JPEG and a 3DS icon a 48x48 PNG, both on black")
{
    std::vector<std::string> notes;
    RenderedIcon forSwitch = Icon::render(Platform::Switch, gradient(256, 256), &notes);
    const std::string &jpeg = forSwitch.m_files.front().m_bytes;
    CHECK(static_cast<unsigned char>(jpeg[0]) == 0xff);
    CHECK(static_cast<unsigned char>(jpeg[1]) == 0xd8);
    Image decoded = decodeImage(jpeg);
    CHECK(decoded.m_width == 256);
    CHECK(decoded.m_height == 256);
    CHECK(decoded.pixel(2, 2)[0] < 8);
    CHECK(notes == std::vector<std::string>{"Flattened onto black, since a JPEG holds no transparency."});

    notes.clear();
    RenderedIcon for3ds = Icon::render(Platform::Nintendo3ds, gradient(256, 256), &notes);
    Image small = decodeImage(for3ds.m_files.front().m_bytes);
    CHECK(small.m_width == 48);
    CHECK(small.m_height == 48);
    CHECK_FALSE(small.m_hasAlpha);
    CHECK(small.pixel(0, 0)[0] == 0);
    CHECK(notes == std::vector<std::string>{"Reduced from 256x256.", "Flattened onto black, since the 3DS stores its icon without transparency."});
}

TEST_CASE("a DS icon is a 32x32 BMP of sixteen colors whose first is the transparent one")
{
    std::vector<std::string> notes;
    RenderedIcon icon = Icon::render(Platform::NintendoDs, gradient(64, 64), &notes);
    const std::string &bmp = icon.m_files.front().m_bytes;

    CHECK(bmp.substr(0, 2) == "BM");
    CHECK(littleEndian(bmp, 2, 4) == bmp.size());
    CHECK(littleEndian(bmp, 10, 4) == 118);
    CHECK(littleEndian(bmp, 18, 4) == 32);
    CHECK(littleEndian(bmp, 22, 4) == 32);
    CHECK(littleEndian(bmp, 28, 2) == 4);
    CHECK(littleEndian(bmp, 30, 4) == 0);
    CHECK(littleEndian(bmp, 46, 4) == 16);
    CHECK(bmp.size() == 118 + 32 * 16);
    CHECK(icon.m_colorCount == 16);
    CHECK(notes == std::vector<std::string>{"Reduced from 64x64.", "Approximated to 15 colors, which is all the DS icon holds besides transparency."});

    // The first row of the file is the bottom of the image, and its last the top, where the transparent corner is.
    auto indexAt = [&bmp](int x, int y) {
        unsigned char byte = static_cast<unsigned char>(bmp[118 + static_cast<std::size_t>(31 - y) * 16 + x / 2]);
        return x % 2 == 0 ? byte >> 4 : byte & 0x0f;
    };
    for (int y = 0; y < 32; ++y)
    {
        for (int x = 0; x < 32; ++x)
        {
            bool isTransparent = icon.m_preview.pixel(x, y)[3] == 0;
            CHECK((indexAt(x, y) == 0) == isTransparent);
        }
    }
    CHECK(indexAt(0, 0) == 0);
    CHECK(indexAt(31, 31) != 0);

    Image decoded = decodeImage(bmp);
    CHECK(decoded.m_width == 32);
    CHECK(decoded.pixel(31, 31)[0] == icon.m_preview.pixel(31, 31)[0]);
}

TEST_CASE("an fx-CG50 icon is a pair of 92x64 PNGs, framed in white and in blue")
{
    std::vector<std::string> notes;
    RenderedIcon icon = Icon::render(Platform::Cg50, flat(164, 108, {200, 0, 0, 255}), &notes);
    REQUIRE(icon.m_files.size() == 2);
    CHECK(icon.m_files[0].m_path == "cg50/icon-uns.png");
    CHECK(icon.m_files[1].m_path == "cg50/icon-sel.png");
    CHECK(notes == std::vector<std::string>{"Reduced from 164x108."});

    Image unselected = decodeImage(icon.m_files[0].m_bytes);
    Image selected = decodeImage(icon.m_files[1].m_bytes);
    for (const Image *image : {&unselected, &selected})
    {
        CHECK(image->m_width == 92);
        CHECK(image->m_height == 64);
        CHECK_FALSE(image->m_hasAlpha);
        CHECK(image->pixel(46, 32)[0] == 200);
        CHECK(image->pixel(5, 5)[0] == 200);
        CHECK(image->pixel(86, 58)[0] == 200);
        // The very corner is outside the round frame, so it is the menu's white.
        CHECK(image->pixel(0, 0)[2] == 255);
    }
    CHECK(unselected.pixel(46, 2)[0] == 255);
    CHECK(selected.pixel(46, 2)[0] == 99);
    CHECK(selected.pixel(46, 2)[1] == 207);
    CHECK(selected.pixel(2, 32)[2] == 255);
}

TEST_CASE("a platform with no icon and a null source render nothing")
{
    std::vector<std::string> notes;
    CHECK(Icon::render(Platform::Web, gradient(64, 64), &notes).isNull());
    CHECK(Icon::render(Platform::Vita, Image(), &notes).isNull());
    CHECK(Icon::describe({}) == "Nothing was written.");
}

TEST_CASE("icons are written into the project, all of them or none")
{
    TemporaryDirectory project("icon-write");
    std::vector<std::string> notes;
    std::vector<RenderedIcon> icons = {Icon::render(Platform::NintendoDs, gradient(64, 64), &notes), Icon::render(Platform::Cg50, gradient(92, 64), &notes),
                                       Icon::render(Platform::Web, gradient(64, 64), &notes)};

    std::string error;
    REQUIRE(Icon::write(project.path(), icons, &error));
    CHECK(project.read("nds/icon.bmp") == icons[0].m_files[0].m_bytes);
    CHECK(project.read("cg50/icon-sel.png") == icons[1].m_files[1].m_bytes);
    CHECK(Icon::describe(icons) ==
          "nds/icon.bmp is now a 32x32 BMP of 16 colors, and cg50/icon-uns.png a 92x64 PNG, with cg50/icon-sel.png its selected twin.");

    int width = 0, height = 0;
    CHECK(imageSize(project.path() + "/nds/icon.bmp", &width, &height));
    CHECK(width == 32);

    // A file where a folder has to be makes the second icon fail, and the first is then left as it was.
    project.write("switch", "in the way");
    std::vector<RenderedIcon> failing = {Icon::render(Platform::NintendoDs, flat(32, 32, {1, 2, 3, 255}), &notes),
                                         Icon::render(Platform::Switch, gradient(64, 64), &notes)};
    CHECK_FALSE(Icon::write(project.path(), failing, &error));
    CHECK_FALSE(error.empty());
    CHECK(project.read("nds/icon.bmp") == icons[0].m_files[0].m_bytes);
    CHECK_FALSE(std::filesystem::exists(project.path() + "/nds/icon.bmp.tmp"));
}

TEST_CASE("an image is read from a file and shrunk into another")
{
    TemporaryDirectory directory("image-files");
    REQUIRE(writeImage(directory.path() + "/big.png", gradient(400, 200)));
    REQUIRE(writeImage(directory.path() + "/photo.jpg", flat(40, 30, {9, 9, 9, 255})));

    std::string error;
    Image read = readImage(directory.path() + "/big.png", &error);
    CHECK(read.m_width == 400);
    CHECK(read.m_hasAlpha);
    CHECK_FALSE(readImage(directory.path() + "/photo.jpg", &error).m_hasAlpha);

    REQUIRE(shrinkImage(directory.path() + "/big.png", directory.path() + "/small.png", 100, &error));
    int width = 0, height = 0;
    REQUIRE(imageSize(directory.path() + "/small.png", &width, &height));
    CHECK(width == 100);
    CHECK(height == 50);

    directory.write("notes.txt", "not an image");
    CHECK(readImage(directory.path() + "/notes.txt", &error).isNull());
    CHECK(error.find("notes.txt could not be read as an image") == 0);
}
