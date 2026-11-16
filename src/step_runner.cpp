#include "fried/core/step_runner.h"

#include <fstream>

namespace fried::core
{
    std::vector<SubprocessOptions> subprocessSteps(const std::vector<BuildStep> &steps, const std::string &workDirectory)
    {
        std::vector<SubprocessOptions> programs;
        for (const BuildStep &step : steps)
        {
            SubprocessOptions options;
            options.m_arguments = step.m_arguments;
            options.m_environment = step.m_environment;
            options.m_workDirectory = workDirectory;
            programs.push_back(options);
        }
        return programs;
    }

    void StepRunner::start(std::vector<SubprocessOptions> steps, const std::string &logFile)
    {
        m_running.reset();
        m_steps = std::move(steps);
        for (SubprocessOptions &step : m_steps)
        {
            step.m_captureOutput = true;
        }
        m_next = 0;
        m_state = State::Running;
        m_failure.clear();
        m_pending.clear();
        m_lines.clear();
        m_logFile = logFile;
        if (!m_logFile.empty())
        {
            std::ofstream(m_logFile, std::ios::trunc);
        }
    }

    std::string StepRunner::stepLabel() const
    {
        if (m_next == 0 || m_next > m_steps.size())
        {
            return std::string();
        }
        const std::vector<std::string> &arguments = m_steps[m_next - 1].m_arguments;
        if (!arguments.empty() && arguments[0] == "haxe")
        {
            return "Generating C++ with Haxe";
        }
        for (const std::string &argument : arguments)
        {
            if (argument == "--build")
            {
                return "Compiling";
            }
        }
        return "Configuring with CMake";
    }

    void StepRunner::append(const std::string &text, LineKind kind)
    {
        m_lines.push_back({text, kind});
        if (!m_logFile.empty())
        {
            std::ofstream(m_logFile, std::ios::app) << text << "\n";
        }
    }

    // A line is only shown once it is whole, except for what a step leaves unfinished when it ends.
    void StepRunner::collect(bool isLast)
    {
        if (m_running)
        {
            m_pending += m_running->readOutput();
        }

        for (std::size_t newline = m_pending.find('\n'); newline != std::string::npos; newline = m_pending.find('\n'))
        {
            std::string line = m_pending.substr(0, newline);
            m_pending.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            append(line, kindOf(line));
        }

        if (isLast && !m_pending.empty())
        {
            append(m_pending, kindOf(m_pending));
            m_pending.clear();
        }
    }

    void StepRunner::fail(const std::string &reason)
    {
        m_failure = reason;
        m_state = State::Failed;
        append(reason, LineKind::Error);
    }

    StepRunner::State StepRunner::pump()
    {
        if (m_state != State::Running)
        {
            return m_state;
        }

        if (m_running)
        {
            if (m_running->isRunning())
            {
                collect(false);
                return m_state;
            }

            collect(true);
            int exitCode = m_running->exitCode();
            m_running.reset();
            if (exitCode != 0)
            {
                fail("Stopped: " + m_steps[m_next - 1].m_arguments[0] + " exited with " + std::to_string(exitCode) + ".");
                return m_state;
            }
        }

        if (m_next == m_steps.size())
        {
            m_state = State::Succeeded;
            append("Completed.", LineKind::Success);
            return m_state;
        }

        BuildStep shown;
        shown.m_arguments = m_steps[m_next].m_arguments;
        append("$ " + Build::commandLine(shown), LineKind::Command);

        std::string error;
        m_running = startSubprocess(m_steps[m_next], &error);
        ++m_next;
        if (!m_running)
        {
            fail(error);
        }
        return m_state;
    }

    void StepRunner::cancel()
    {
        if (m_state != State::Running)
        {
            return;
        }
        if (m_running)
        {
            m_running->terminate();
            collect(true);
            m_running.reset();
        }
        m_state = State::Cancelled;
        m_failure = "Cancelled.";
        append(m_failure, LineKind::Warning);
    }
}
