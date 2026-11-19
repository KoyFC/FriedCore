#include "fried/core/process.h"
#include "fried/core/toolchains.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <cstring>

namespace fried::core
{
    namespace
    {
        std::wstring wide(const std::string &text)
        {
            if (text.empty())
            {
                return std::wstring();
            }
            int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
            std::wstring result(static_cast<std::size_t>(size), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
            return result;
        }

        std::string narrow(const std::wstring &text)
        {
            if (text.empty())
            {
                return std::string();
            }
            int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
            std::string result(static_cast<std::size_t>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
            return result;
        }

        std::string lastError()
        {
            wchar_t *buffer = nullptr;
            DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                                          GetLastError(), 0, reinterpret_cast<wchar_t *>(&buffer), 0, nullptr);
            std::wstring message(buffer ? buffer : L"", length);
            if (buffer)
            {
                LocalFree(buffer);
            }
            while (!message.empty() && (message.back() == L'\n' || message.back() == L'\r' || message.back() == L' '))
            {
                message.pop_back();
            }
            return narrow(message);
        }

        // The rules CommandLineToArgvW reads a command line by, which is how the program splits it again.
        std::wstring quotedArgument(const std::wstring &argument)
        {
            if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
            {
                return argument;
            }

            std::wstring quoted = L"\"";
            for (std::size_t index = 0;; ++index)
            {
                std::size_t backslashes = 0;
                while (index < argument.size() && argument[index] == L'\\')
                {
                    ++index;
                    ++backslashes;
                }
                if (index == argument.size())
                {
                    quoted.append(backslashes * 2, L'\\');
                    break;
                }
                if (argument[index] == L'"')
                {
                    quoted.append(backslashes * 2 + 1, L'\\');
                }
                else
                {
                    quoted.append(backslashes, L'\\');
                }
                quoted.push_back(argument[index]);
            }
            quoted.push_back(L'"');
            return quoted;
        }

        std::wstring lowered(std::wstring text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](wchar_t letter) { return static_cast<wchar_t>(std::towlower(letter)); });
            return text;
        }

        bool isBatchFile(const std::wstring &program)
        {
            std::wstring name = lowered(program);
            return name.size() > 4 && (name.compare(name.size() - 4, 4, L".bat") == 0 || name.compare(name.size() - 4, 4, L".cmd") == 0);
        }

        // Names are the same variable whatever their case on Windows, so Path and PATH replace each other.
        std::wstring environmentBlock(const std::map<std::string, std::string> &added)
        {
            std::map<std::wstring, std::wstring> variables;
            wchar_t *inherited = GetEnvironmentStringsW();
            for (const wchar_t *entry = inherited; entry && *entry; entry += wcslen(entry) + 1)
            {
                std::wstring text = entry;
                std::size_t separator = text.find(L'=', 1);
                if (separator != std::wstring::npos)
                {
                    variables[lowered(text.substr(0, separator))] = text;
                }
            }
            if (inherited)
            {
                FreeEnvironmentStringsW(inherited);
            }
            for (const auto &[name, value] : added)
            {
                variables[lowered(wide(name))] = wide(name) + L"=" + wide(value);
            }

            std::wstring block;
            for (const auto &entry : variables)
            {
                block += entry.second;
                block.push_back(L'\0');
            }
            block.push_back(L'\0');
            return block;
        }

        class WindowsSubprocess : public Subprocess
        {
        public:
            WindowsSubprocess(HANDLE process, HANDLE job, HANDLE output)
                : m_process(process)
                , m_job(job)
                , m_output(output)
            {
            }

            // Closing the job ends whatever the program left running, so closing a tool never leaves a build behind.
            ~WindowsSubprocess() override
            {
                terminate();
                WaitForSingleObject(m_process, 2000);
                if (m_output)
                {
                    CloseHandle(m_output);
                }
                CloseHandle(m_process);
                CloseHandle(m_job);
            }

            bool isRunning() override
            {
                if (m_hasExited)
                {
                    return false;
                }
                if (WaitForSingleObject(m_process, 0) == WAIT_TIMEOUT)
                {
                    return true;
                }
                DWORD code = 0;
                m_exitCode = GetExitCodeProcess(m_process, &code) ? static_cast<int>(code) : -1;
                m_hasExited = true;
                return false;
            }

            int exitCode() const override { return m_exitCode; }

            // There is no gentler request than this on Windows, and it stops the program and what it started together.
            void terminate() override
            {
                if (isRunning())
                {
                    TerminateJobObject(m_job, 1);
                }
            }

            std::string readOutput() override
            {
                std::string text;
                if (!m_output)
                {
                    return text;
                }
                char buffer[4096];
                for (;;)
                {
                    DWORD available = 0;
                    if (!PeekNamedPipe(m_output, nullptr, 0, nullptr, &available, nullptr) || available == 0)
                    {
                        break;
                    }
                    DWORD count = 0;
                    if (!ReadFile(m_output, buffer, std::min<DWORD>(available, sizeof(buffer)), &count, nullptr) || count == 0)
                    {
                        break;
                    }
                    text.append(buffer, count);
                }
                return text;
            }

        private:
            HANDLE m_process;
            HANDLE m_job;
            HANDLE m_output;
            int m_exitCode = -1;
            bool m_hasExited = false;
        };

        HANDLE openForChild(const wchar_t *path, DWORD access, DWORD disposition)
        {
            SECURITY_ATTRIBUTES inheritable = {sizeof(inheritable), nullptr, TRUE};
            return CreateFileW(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
        }
    }

    std::unique_ptr<Subprocess> startSubprocess(const SubprocessOptions &options, std::string *error)
    {
        const std::vector<std::string> &arguments = options.m_arguments;
        if (arguments.empty())
        {
            *error = "There is no program to run.";
            return nullptr;
        }

        std::string program = arguments[0];
        if (program.find('\\') == std::string::npos && program.find('/') == std::string::npos)
        {
            program = findExecutable(program);
            if (program.empty())
            {
                *error = "Cannot run " + arguments[0] + ": it is not on the PATH.";
                return nullptr;
            }
        }
        std::replace(program.begin(), program.end(), '/', '\\');

        std::wstring application = wide(program);
        std::wstring commandLine = quotedArgument(application);
        for (std::size_t index = 1; index < arguments.size(); ++index)
        {
            commandLine += L" " + quotedArgument(wide(arguments[index]));
        }
        if (isBatchFile(application))
        {
            wchar_t shell[MAX_PATH] = {};
            GetEnvironmentVariableW(L"ComSpec", shell, MAX_PATH);
            application = shell;
            commandLine = L"\"" + std::wstring(shell) + L"\" /d /s /c \"" + commandLine + L"\"";
        }

        HANDLE input = openForChild(L"NUL", GENERIC_READ, OPEN_EXISTING);
        HANDLE readEnd = nullptr;
        HANDLE output = INVALID_HANDLE_VALUE;
        if (options.m_captureOutput)
        {
            SECURITY_ATTRIBUTES inheritable = {sizeof(inheritable), nullptr, TRUE};
            if (CreatePipe(&readEnd, &output, &inheritable, 0))
            {
                SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
            }
        }
        else
        {
            std::wstring log = options.m_logFile.empty() ? L"NUL" : wide(options.m_logFile);
            output = openForChild(log.c_str(), GENERIC_WRITE, options.m_logFile.empty() ? OPEN_EXISTING : CREATE_ALWAYS);
        }

        auto closeAll = [&] {
            for (HANDLE handle : {input, output, readEnd})
            {
                if (handle && handle != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(handle);
                }
            }
        };
        if (input == INVALID_HANDLE_VALUE || output == INVALID_HANDLE_VALUE)
        {
            *error = "Cannot run " + arguments[0] + ": " + lastError();
            closeAll();
            return nullptr;
        }

        // Only these two are inherited, so nothing else the tool holds open, such as its socket to the player, reaches the program.
        HANDLE inherited[2] = {input, output};
        SIZE_T attributesSize = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attributesSize);
        std::vector<char> attributesStorage(attributesSize);
        LPPROC_THREAD_ATTRIBUTE_LIST attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributesStorage.data());
        InitializeProcThreadAttributeList(attributes, 1, 0, &attributesSize);
        UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr);

        STARTUPINFOEXW startup = {};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = input;
        startup.StartupInfo.hStdOutput = output;
        startup.StartupInfo.hStdError = output;
        startup.lpAttributeList = attributes;

        std::wstring environment = environmentBlock(options.m_environment);
        std::wstring directory = wide(options.m_workDirectory);
        PROCESS_INFORMATION process = {};
        BOOL started = CreateProcessW(application.c_str(), commandLine.data(), nullptr, nullptr, TRUE,
                                      CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW | CREATE_SUSPENDED,
                                      environment.data(), directory.empty() ? nullptr : directory.c_str(), &startup.StartupInfo, &process);
        std::string failure = started ? std::string() : lastError();
        DeleteProcThreadAttributeList(attributes);

        HANDLE job = nullptr;
        if (started)
        {
            job = CreateJobObjectW(nullptr, nullptr);
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
            AssignProcessToJobObject(job, process.hProcess);
            ResumeThread(process.hThread);
            CloseHandle(process.hThread);
        }

        // The program has its own copies now.
        HANDLE ours = options.m_captureOutput ? readEnd : nullptr;
        if (input != INVALID_HANDLE_VALUE)
        {
            CloseHandle(input);
        }
        CloseHandle(output);
        if (!started)
        {
            if (readEnd)
            {
                CloseHandle(readEnd);
            }
            *error = "Cannot run " + arguments[0] + ": " + failure;
            return nullptr;
        }
        return std::make_unique<WindowsSubprocess>(process.hProcess, job, ours);
    }
}
