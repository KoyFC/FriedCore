// fried_migrate <project directory>
//
// Rewrites a project's project file, scenes and import settings from JSON to YAML, keeping the originals in .fried/json-backup.
#include "fried/core/migration.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: fried_migrate <project directory>\n";
        return 2;
    }
    if (!fried::core::needsYamlMigration(argv[1]))
    {
        std::cout << "Nothing to convert: the project is already written in YAML.\n";
        return 0;
    }
    fried::core::MigrationReport report;
    std::string error;
    if (!fried::core::migrateToYaml(argv[1], &report, &error))
    {
        std::cerr << error << "\n";
        return 1;
    }
    for (const std::string &file : report.m_converted)
    {
        std::cout << "converted " << file << "\n";
    }
    for (const std::string &failure : report.m_failed)
    {
        std::cerr << "left alone " << failure << "\n";
    }
    std::cout << "The originals are in " << report.m_backupDirectory << "\n";
    return report.m_failed.empty() ? 0 : 1;
}
