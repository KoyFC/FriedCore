#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fried::core
{
    // A program started by a tool, which ends with the object that holds it.
    class Subprocess
    {
    public:
        virtual ~Subprocess() = default;

        virtual bool isRunning() = 0;
        virtual int exitCode() const = 0;

        // Asks the program, and everything it started, to stop.
        virtual void terminate() = 0;

        // What the program has printed since the last call, when its output is captured. Never blocks, and still gives what is left after the program has ended.
        virtual std::string readOutput() = 0;
    };

    struct SubprocessOptions
    {
        std::vector<std::string> m_arguments;

        // Added to the environment of the tool, which the program inherits.
        std::map<std::string, std::string> m_environment;
        std::string m_workDirectory;

        // Receives what the program prints, standard output and error together. Empty discards it.
        std::string m_logFile;

        // Keeps what the program prints for readOutput instead, and the log file is then not written.
        bool m_captureOutput = false;
    };

    std::unique_ptr<Subprocess> startSubprocess(const SubprocessOptions &options, std::string *error);
}
