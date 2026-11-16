#pragma once

#include "fried/core/build.h"
#include "fried/core/line_kind.h"
#include "fried/core/process.h"

#include <memory>
#include <string>
#include <vector>

namespace fried::core
{
    // A build's steps as programs to run in the project's folder.
    std::vector<SubprocessOptions> subprocessSteps(const std::vector<BuildStep> &steps, const std::string &workDirectory);

    // Runs programs one after another without blocking the tool, and keeps what they print as lines.
    class StepRunner
    {
    public:
        enum class State
        {
            Idle,
            Running,
            Succeeded,
            Failed,
            Cancelled
        };

        // Every line is also added to the log file, when one is named.
        void start(std::vector<SubprocessOptions> steps, const std::string &logFile = std::string());

        // Advances the run and says where it stands. A step that fails stops it.
        State pump();
        State state() const { return m_state; }

        // Stops the running step and everything it started.
        void cancel();

        const std::vector<OutputLine> &lines() const { return m_lines; }

        // Which step is running, from 1, how many there are, and what it does in words.
        std::size_t step() const { return m_next; }
        std::size_t stepCount() const { return m_steps.size(); }
        std::string stepLabel() const;

        // What stopped the run, once something has.
        const std::string &failure() const { return m_failure; }

    private:
        void collect(bool isLast);
        void append(const std::string &text, LineKind kind);
        void fail(const std::string &reason);

        std::vector<SubprocessOptions> m_steps;
        std::size_t m_next = 0;
        std::unique_ptr<Subprocess> m_running;
        State m_state = State::Idle;
        std::string m_failure;
        std::string m_logFile;

        // The end of the output that has no line break yet.
        std::string m_pending;
        std::vector<OutputLine> m_lines;
    };
}
