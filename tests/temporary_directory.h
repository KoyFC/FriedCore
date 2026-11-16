#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// A folder of its own for one test, removed with everything in it when the test ends.
class TemporaryDirectory
{
public:
    explicit TemporaryDirectory(const std::string &name)
        : m_path(std::filesystem::temp_directory_path() / ("fried-core-test-" + name))
    {
        std::filesystem::remove_all(m_path);
        std::filesystem::create_directories(m_path);
    }

    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(m_path, ignored);
    }

    std::string path() const { return m_path.string(); }

    void write(const std::string &relative, const std::string &text) const
    {
        std::filesystem::path file = m_path / relative;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary) << text;
    }

    std::string read(const std::string &relative) const { return readFile((m_path / relative).string()); }

    static std::string readFile(const std::string &file)
    {
        std::ifstream stream(file, std::ios::binary);
        std::stringstream text;
        text << stream.rdbuf();
        return text.str();
    }

private:
    std::filesystem::path m_path;
};
