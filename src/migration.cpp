#include "fried/core/migration.h"
#include "fried/core/project.h"
#include "fried/core/scene_files.h"
#include "fried/core/yaml.h"
#include "paths.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace fried::core
{
    namespace
    {
        struct Candidate
        {
            fs::path m_path;
            bool m_isScene;
        };

        bool startsWithBrace(const fs::path &path)
        {
            std::ifstream file(path, std::ios::binary);
            char c = 0;
            while (file.get(c))
            {
                if (c == '\xEF' || c == '\xBB' || c == '\xBF' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
                {
                    continue;
                }
                return c == '{';
            }
            return false;
        }

        bool hasExtension(const fs::path &path, const std::string &extension)
        {
            return path.extension().string() == extension;
        }

        std::vector<Candidate> candidates(const std::string &projectDirectory)
        {
            std::vector<Candidate> found;
            fs::path root = pathOf(projectDirectory);
            std::error_code ignored;
            if (fs::exists(root / Project::s_fileName, ignored) && startsWithBrace(root / Project::s_fileName))
            {
                found.push_back({root / Project::s_fileName, false});
            }
            fs::path assets = root / "assets";
            if (fs::exists(assets, ignored))
            {
                for (fs::recursive_directory_iterator entry(assets, ignored), end; entry != end; entry.increment(ignored))
                {
                    if (!entry->is_regular_file(ignored))
                    {
                        continue;
                    }
                    std::string name = entry->path().filename().string();
                    if (name.size() > std::string(s_legacySceneSuffix).size() && name.compare(name.size() - 11, 11, s_legacySceneSuffix) == 0)
                    {
                        found.push_back({entry->path(), true});
                    }
                    else if (hasExtension(entry->path(), ".import") && startsWithBrace(entry->path()))
                    {
                        found.push_back({entry->path(), false});
                    }
                }
            }
            std::sort(found.begin(), found.end(), [](const Candidate &a, const Candidate &b) { return a.m_path < b.m_path; });
            return found;
        }

        std::string readText(const fs::path &path)
        {
            std::ifstream file(path, std::ios::binary);
            std::stringstream text;
            text << file.rdbuf();
            return text.str();
        }

        bool writeAtomically(const fs::path &target, const std::string &text)
        {
            fs::path staged = target;
            staged += ".tmp";
            {
                std::ofstream file(staged, std::ios::binary | std::ios::trunc);
                file << text;
                file.flush();
                if (!file)
                {
                    std::error_code ignored;
                    fs::remove(staged, ignored);
                    return false;
                }
            }
            std::error_code failure;
            fs::rename(staged, target, failure);
            if (failure)
            {
                fs::remove(staged, failure);
                return false;
            }
            return true;
        }

        std::string timestamp()
        {
            std::time_t now = std::time(nullptr);
            char buffer[32];
            std::strftime(buffer, sizeof buffer, "%Y%m%d-%H%M%S", std::localtime(&now));
            return buffer;
        }
    }

    bool needsYamlMigration(const std::string &projectDirectory)
    {
        return !candidates(projectDirectory).empty();
    }

    bool migrateToYaml(const std::string &projectDirectory, MigrationReport *report, std::string *error)
    {
        std::vector<Candidate> files = candidates(projectDirectory);
        fs::path root = pathOf(projectDirectory);
        fs::path backup = root / ".fried" / "json-backup" / timestamp();
        report->m_backupDirectory = textOf(fs::relative(backup, root));
        if (files.empty())
        {
            return true;
        }

        std::error_code failure;
        for (const Candidate &candidate : files)
        {
            fs::path copy = backup / fs::relative(candidate.m_path, root);
            fs::create_directories(copy.parent_path(), failure);
            if (!failure)
            {
                fs::copy_file(candidate.m_path, copy, fs::copy_options::overwrite_existing, failure);
            }
            if (failure)
            {
                *error = "Could not copy " + textOf(candidate.m_path) + " to " + textOf(backup) + ": " + failure.message();
                return false;
            }
        }

        for (const Candidate &candidate : files)
        {
            std::string relative = textOf(fs::relative(candidate.m_path, root));
            Json data;
            std::string problem;
            if (!parseYaml(readText(candidate.m_path), &data, &problem))
            {
                report->m_failed.push_back(relative + ": " + problem);
                continue;
            }
            fs::path target = candidate.m_path;
            if (candidate.m_isScene)
            {
                std::string name = sceneName(candidate.m_path.filename().string());
                target = candidate.m_path.parent_path() / (name + s_sceneSuffix);
                if (fs::exists(target))
                {
                    report->m_failed.push_back(relative + ": " + name + s_sceneSuffix + " exists too, so this file was left alone");
                    continue;
                }
            }
            if (!writeAtomically(target, emitYaml(data, candidate.m_isScene ? YamlLayout::Scene : YamlLayout::Plain)))
            {
                report->m_failed.push_back(relative + ": the YAML could not be written");
                continue;
            }
            if (candidate.m_isScene)
            {
                fs::remove(candidate.m_path, failure);
            }
            report->m_converted.push_back(relative);
        }
        return true;
    }
}
