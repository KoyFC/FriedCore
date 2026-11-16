#include "fried/core/process.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char **environ;

namespace fried::core
{
    namespace
    {
        class PosixSubprocess : public Subprocess
        {
        public:
            PosixSubprocess(pid_t process, int output)
                : m_process(process)
                , m_output(output)
                , m_exitCode(-1)
                , m_hasExited(false)
            {
            }

            // A program that ignores being asked is killed, so closing a tool never waits on it for long.
            ~PosixSubprocess() override
            {
                terminate();
                for (int attempt = 0; attempt < 200 && isRunning(); ++attempt)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                if (!m_hasExited)
                {
                    kill(-m_process, SIGKILL);
                    waitpid(m_process, nullptr, 0);
                }
                if (m_output >= 0)
                {
                    close(m_output);
                }
            }

            bool isRunning() override
            {
                if (m_hasExited)
                {
                    return false;
                }
                int status = 0;
                pid_t result = waitpid(m_process, &status, WNOHANG);
                if (result == 0)
                {
                    return true;
                }
                m_hasExited = true;
                m_exitCode = result == m_process && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                return false;
            }

            int exitCode() const override { return m_exitCode; }

            // The program leads a group of its own, so what it started stops with it.
            void terminate() override
            {
                if (isRunning())
                {
                    kill(-m_process, SIGTERM);
                }
            }

            std::string readOutput() override
            {
                std::string text;
                if (m_output < 0)
                {
                    return text;
                }
                char buffer[4096];
                for (;;)
                {
                    ssize_t count = read(m_output, buffer, sizeof(buffer));
                    if (count > 0)
                    {
                        text.append(buffer, static_cast<std::size_t>(count));
                    }
                    else if (count < 0 && errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }
                return text;
            }

        private:
            pid_t m_process;
            int m_output;
            int m_exitCode;
            bool m_hasExited;
        };
    }

    std::unique_ptr<Subprocess> startSubprocess(const SubprocessOptions &options, std::string *error)
    {
        const std::vector<std::string> &arguments = options.m_arguments;
        const std::map<std::string, std::string> &environment = options.m_environment;
        if (arguments.empty())
        {
            *error = "There is no program to run.";
            return nullptr;
        }

        std::vector<std::string> variables;
        for (char **entry = environ; *entry; ++entry)
        {
            std::string text = *entry;
            if (environment.find(text.substr(0, text.find('='))) == environment.end())
            {
                variables.push_back(text);
            }
        }
        for (const auto &[name, value] : environment)
        {
            variables.push_back(name + "=" + value);
        }

        std::vector<char *> argv;
        for (const std::string &argument : arguments)
        {
            argv.push_back(const_cast<char *>(argument.c_str()));
        }
        argv.push_back(nullptr);
        std::vector<char *> envp;
        for (const std::string &variable : variables)
        {
            envp.push_back(const_cast<char *>(variable.c_str()));
        }
        envp.push_back(nullptr);

        int ends[2] = {-1, -1};
        if (options.m_captureOutput && pipe(ends) != 0)
        {
            *error = "Cannot run " + arguments[0] + ": " + std::strerror(errno);
            return nullptr;
        }

        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        if (!options.m_workDirectory.empty())
        {
            posix_spawn_file_actions_addchdir_np(&actions, options.m_workDirectory.c_str());
        }
        posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        if (options.m_captureOutput)
        {
            posix_spawn_file_actions_adddup2(&actions, ends[1], STDOUT_FILENO);
            posix_spawn_file_actions_addclose(&actions, ends[0]);
            posix_spawn_file_actions_addclose(&actions, ends[1]);
        }
        else
        {
            posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, options.m_logFile.empty() ? "/dev/null" : options.m_logFile.c_str(),
                                             O_WRONLY | O_CREAT | O_TRUNC, 0644);
        }
        posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);

        posix_spawnattr_t attributes;
        posix_spawnattr_init(&attributes);
        posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
        posix_spawnattr_setpgroup(&attributes, 0);

        pid_t process = 0;
        int result = posix_spawnp(&process, arguments[0].c_str(), &actions, &attributes, argv.data(), envp.data());
        posix_spawn_file_actions_destroy(&actions);
        posix_spawnattr_destroy(&attributes);

        if (ends[1] >= 0)
        {
            close(ends[1]);
        }
        if (result != 0)
        {
            if (ends[0] >= 0)
            {
                close(ends[0]);
            }
            *error = "Cannot run " + arguments[0] + ": " + std::strerror(result);
            return nullptr;
        }

        if (ends[0] >= 0)
        {
            fcntl(ends[0], F_SETFL, fcntl(ends[0], F_GETFL) | O_NONBLOCK);
            fcntl(ends[0], F_SETFD, FD_CLOEXEC);
        }
        return std::make_unique<PosixSubprocess>(process, ends[0]);
    }
}
