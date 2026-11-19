#include "doctest/doctest.h"

#include "fried/core/step_runner.h"
#include "temporary_directory.h"

#include <chrono>
#include <thread>

#ifndef _WIN32
#include <csignal>
#include <sys/types.h>
#endif

using namespace fried::core;

namespace
{
#ifdef _WIN32
    SubprocessOptions command(const std::string &line)
    {
        SubprocessOptions options;
        options.m_arguments = {"cmd.exe", "/d", "/c", line};
        return options;
    }
#else
    SubprocessOptions shell(const std::string &script)
    {
        SubprocessOptions options;
        options.m_arguments = {"/bin/sh", "-c", script};
        return options;
    }
#endif

    StepRunner::State finish(StepRunner &runner)
    {
        for (int attempt = 0; attempt < 1000 && runner.pump() == StepRunner::State::Running; ++attempt)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return runner.state();
    }

    std::vector<std::string> textOf(const StepRunner &runner)
    {
        std::vector<std::string> text;
        for (const OutputLine &line : runner.lines())
        {
            text.push_back(line.m_text);
        }
        return text;
    }
}

TEST_CASE("a line of build output is known by what the tools print")
{
    CHECK(kindOf("main.cpp:3:5: error: expected ';'") == LineKind::Error);
    CHECK(kindOf("main.cpp:3:5: fatal Error: no such file") == LineKind::Error);
    CHECK(kindOf("CMake Error at CMakeLists.txt:4 (message):") == LineKind::Error);
    CHECK(kindOf("ld: main.o: undefined reference to `start'") == LineKind::Error);
    CHECK(kindOf("src/Main.hx:12: characters 9-14 : Type not found : Thing") == LineKind::Error);
    CHECK(kindOf("see src/Main.hx:12: for more") == LineKind::Plain);

    CHECK(kindOf("main.cpp:3:5: warning: unused variable") == LineKind::Warning);
    CHECK(kindOf("CMake Warning (dev) at CMakeLists.txt:1:") == LineKind::Warning);
    CHECK(kindOf("Warning : this is deprecated") == LineKind::Warning);

    CHECK(kindOf("[ 50%] Building CXX object main.cpp.o") == LineKind::Progress);
    CHECK(kindOf("-- Configuring done") == LineKind::Progress);
    CHECK(kindOf("Scanning dependencies") == LineKind::Plain);
}

#ifndef _WIN32
TEST_CASE("steps run in order and their output arrives as whole lines")
{
    StepRunner runner;
    runner.start({shell("printf 'one\\ntwo'; printf ' more\\r\\n'"), shell("echo three; echo unfinished | tr -d '\\n'")});
    CHECK(finish(runner) == StepRunner::State::Succeeded);
    CHECK(textOf(runner) == std::vector<std::string>{"$ /bin/sh -c \"printf 'one\\ntwo'; printf ' more\\r\\n'\"", "one", "two more",
                                                     "$ /bin/sh -c \"echo three; echo unfinished | tr -d '\\n'\"", "three", "unfinished", "Completed."});
    CHECK(runner.lines().front().m_kind == LineKind::Command);
    CHECK(runner.lines().back().m_kind == LineKind::Success);
    CHECK(runner.stepCount() == 2);
}

TEST_CASE("a step that fails stops the run and says which program it was")
{
    StepRunner runner;
    runner.start({shell("echo first; echo 'boom: error: bad' >&2; exit 3"), shell("echo never")});
    CHECK(finish(runner) == StepRunner::State::Failed);
    CHECK(runner.failure() == "Stopped: /bin/sh exited with 3.");
    CHECK(runner.step() == 1);

    std::vector<std::string> text = textOf(runner);
    CHECK(text.back() == runner.failure());
    CHECK(runner.lines()[2].m_text == "boom: error: bad");
    CHECK(runner.lines()[2].m_kind == LineKind::Error);
}

TEST_CASE("a program that cannot start fails the run")
{
    StepRunner runner;
    SubprocessOptions missing;
    missing.m_arguments = {"fried-core-no-such-program"};
    runner.start({missing});
    CHECK(finish(runner) == StepRunner::State::Failed);
    CHECK(runner.failure().find("Cannot run fried-core-no-such-program") == 0);
}

TEST_CASE("steps run in the folder and with the environment they are given")
{
    TemporaryDirectory directory("step-runner");
    BuildStep step;
    step.m_arguments = {"/bin/sh", "-c", "echo $FRIED_TEST_VALUE > here.txt"};
    step.m_environment["FRIED_TEST_VALUE"] = "set";

    StepRunner runner;
    runner.start(subprocessSteps({step}, directory.path()), directory.path() + "/log.txt");
    CHECK(finish(runner) == StepRunner::State::Succeeded);
    CHECK(directory.read("here.txt") == "set\n");
    CHECK(directory.read("log.txt") == "$ /bin/sh -c \"echo $FRIED_TEST_VALUE > here.txt\"\nCompleted.\n");
}

TEST_CASE("cancelling stops the program and what it started, at once")
{
    TemporaryDirectory directory("step-runner-cancel");
    StepRunner runner;
    runner.start({shell("sleep 30 & echo $! > " + directory.path() + "/child; echo started; wait"), shell("echo never")});

    auto began = std::chrono::steady_clock::now();
    while (runner.lines().size() < 2 && std::chrono::steady_clock::now() - began < std::chrono::seconds(5))
    {
        runner.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(runner.lines().size() == 2);

    auto asked = std::chrono::steady_clock::now();
    runner.cancel();
    CHECK(std::chrono::steady_clock::now() - asked < std::chrono::seconds(1));
    CHECK(runner.state() == StepRunner::State::Cancelled);
    CHECK(runner.pump() == StepRunner::State::Cancelled);
    CHECK(textOf(runner).back() == "Cancelled.");

    pid_t child = std::stoi(directory.read("child"));
    bool isGone = false;
    for (int attempt = 0; attempt < 200 && !isGone; ++attempt)
    {
        isGone = kill(child, 0) != 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(isGone);
}
#endif

#ifdef _WIN32
TEST_CASE("steps run in order and their output arrives as whole lines")
{
    StepRunner runner;
    runner.start({command("echo one&& echo two"), command("echo three")});
    CHECK(finish(runner) == StepRunner::State::Succeeded);
    std::vector<std::string> text = textOf(runner);
    CHECK(text.back() == "Completed.");
    CHECK(std::vector<std::string>(text.begin() + 1, text.begin() + 3) == std::vector<std::string>{"one", "two"});
    CHECK(runner.stepCount() == 2);
}

TEST_CASE("a step that fails stops the run and says which program it was")
{
    StepRunner runner;
    runner.start({command("exit 3"), command("echo never")});
    CHECK(finish(runner) == StepRunner::State::Failed);
    CHECK(runner.failure() == "Stopped: cmd.exe exited with 3.");
    CHECK(runner.step() == 1);
}

TEST_CASE("steps run in the folder and with the environment they are given")
{
    TemporaryDirectory directory("step-runner");
    BuildStep step;
    step.m_arguments = {"cmd.exe", "/d", "/c", "echo %FRIED_TEST_VALUE%> here.txt"};
    step.m_environment["FRIED_TEST_VALUE"] = "set";

    StepRunner runner;
    runner.start(subprocessSteps({step}, directory.path()));
    CHECK(finish(runner) == StepRunner::State::Succeeded);
    CHECK(directory.read("here.txt") == "set\r\n");
}

TEST_CASE("cancelling stops the program and what it started, at once")
{
    StepRunner runner;
    runner.start({command("ping -n 30 127.0.0.1 >nul"), command("echo never")});

    auto began = std::chrono::steady_clock::now();
    while (runner.lines().size() < 1 && std::chrono::steady_clock::now() - began < std::chrono::seconds(2))
    {
        runner.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    auto asked = std::chrono::steady_clock::now();
    runner.cancel();
    CHECK(std::chrono::steady_clock::now() - asked < std::chrono::seconds(1));
    CHECK(runner.state() == StepRunner::State::Cancelled);
    CHECK(textOf(runner).back() == "Cancelled.");
}
#endif
