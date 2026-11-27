#pragma once

#include <string>
#include <vector>

namespace fried::core
{
    struct MigrationReport
    {
        // Paths relative to the project of the files that were rewritten.
        std::vector<std::string> m_converted;
        // What could not be rewritten, each with the reason. Those files are left as they were.
        std::vector<std::string> m_failed;
        // Where the originals were copied to before anything changed, relative to the project.
        std::string m_backupDirectory;
    };

    // Whether the project still has its project file, its scenes or its import settings written in JSON.
    bool needsYamlMigration(const std::string &projectDirectory);

    // Rewrites them in YAML. The originals are copied to .fried/json-backup/<date and time>/ first, and a file that cannot be converted is left alone and reported. Returns false, with nothing changed, when the backup cannot be made.
    bool migrateToYaml(const std::string &projectDirectory, MigrationReport *report, std::string *error);
}
