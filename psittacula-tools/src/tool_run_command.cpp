#include "tool_run_command.h"
#include "working_dir.h"
#include "format_util.h"
#include "console_writer.h"
#include "utf8_util.h"
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <cstdlib>

namespace {

/// Appends data to dest, keeping at most max_bytes; sets truncated when data is discarded
void AppendCapped(std::string &dest, const char *data, size_t len, size_t max_bytes, bool &truncated)
{
    if (dest.size() >= max_bytes)
    {
        truncated = true;
        return;
    }

    size_t space = max_bytes - dest.size();
    dest.append(data, std::min(len, space));

    if (dest.size() >= max_bytes)
        truncated = true;
}

} // namespace

void RunCommandTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "command",
        "string",
        "Shell command line to execute (interpreted by cmd.exe on Windows or /bin/sh on POSIX).",
        true
        });

    params_acc.push_back({
        "cwd",
        "string",
        "Working directory for the command. Must be inside the project directory. Defaults to the project directory.",
        false
        });

    params_acc.push_back({
        "timeout_seconds",
        "number",
        "Maximum execution time in seconds (1-3600). The process is terminated on timeout. Default: 120.",
        false
        });

    params_acc.push_back({
        "max_output_bytes",
        "number",
        "Maximum captured size of each output stream in bytes (256-10485760). Excess output is discarded and 'truncated' is set to true. Default: 100000.",
        false
        });

    params_acc.push_back({
        "stdin_data",
        "string",
        "Optional data written to the standard input of the process.",
        false
        });
}

std::string RunCommandTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("Run command tool.", console::TextOrigin::tools);

    std::string command = GetParam(params_values, "command");
    std::string cwd = GetParam(params_values, "cwd");
    std::string stdin_data = GetParam(params_values, "stdin_data");

    int timeout_seconds = 120;
    {
        std::string timeout_str = GetParam(params_values, "timeout_seconds");
        if (!timeout_str.empty())
            timeout_seconds = std::atoi(timeout_str.c_str());
        timeout_seconds = std::max(1, std::min(timeout_seconds, 3600));
    }

    size_t max_output_bytes = 100000;
    {
        std::string max_output_str = GetParam(params_values, "max_output_bytes");
        if (!max_output_str.empty())
        {
            long parsed = std::atol(max_output_str.c_str());
            if (parsed > 0)
                max_output_bytes = static_cast<size_t>(parsed);
        }
        max_output_bytes = std::max<size_t>(256, std::min<size_t>(max_output_bytes, 10485760));
    }

    if (command.empty())
    {
        console::write_line("Missing required parameter: command", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: command"}})";
    }

    if (cwd.empty())
    {
        cwd = wdir.GetProjectDir();
    }
    else
    {
        UnEscapeSlashesInPath(cwd);
    }

    console::write_line(fmt.Format("Command: %? %?", command, stdin_data), console::TextOrigin::tools);
    console::write_line(fmt.Format("Working directory: %?", cwd), console::TextOrigin::tools);

    // Make confirm
    bool confirmed_action = false;
    if (auto it = m_safe_commands.find(command); it != m_safe_commands.end())
    {
        confirmed_action = true;
    }

    if (!confirmed_action)
    {
        if(!m_confirmation_window)
            confirmed_action = ConfirmOperation(command);
        else
            confirmed_action = m_confirmation_window->Confirm(fmt.Format("Confirm executing the command:\n%?\n\nWorking directory:\n%?", command, cwd));
    }

    if(!confirmed_action)
    {
        console::write_line("Operation cancelled by user.", console::TextOrigin::tools);
        return R"({"error":{"type":"operation_cancelled","message":"Operation cancelled by user"}})";
    }

    if (!wdir.IsInWorkDir(cwd))
    {
        console::write_line(fmt.Format("Permission_denied: working directory is outside the project directory:\n cwd: %?\n project directory: %?", cwd, wdir.GetProjectDir()), console::TextOrigin::error);
        return fmt.Format("{\"error\":{\"type\":\"permission_denied\",\"message\":\"Working directory is outside the project directory (%?)\",\"cwd\":\"%?\"}}", wdir.GetProjectDir(), cwd);
    }

    if (!std::filesystem::exists(cwd))
    {
        console::write_line(fmt.Format("Working directory does not exist: %?", cwd), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"not_found\",\"message\":\"Working directory does not exist\",\"cwd\":\"%?\"}}",
            cwd
        );
    }

    if (!std::filesystem::is_directory(cwd))
    {
        console::write_line(fmt.Format("Working directory path is not a directory: %?", cwd), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"invalid_arguments\",\"message\":\"Working directory path is not a directory\",\"cwd\":\"%?\"}}",
            cwd
        );
    }

    ProcessResult result;
    auto start_time = std::chrono::steady_clock::now();

    if (!ExecuteProcess(command, cwd, stdin_data, max_output_bytes, timeout_seconds, result))
    {
        console::write_line("Failed to start process", console::TextOrigin::error);
        return R"({"error":{"type":"runtime_error","message":"Failed to start process"}})";
    }

    long long duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();

    // Child processes write in the console code page (CP866 / CP1251 / ...), which is not
    // UTF-8. Raw bytes must never reach the request body: the server rejects the whole
    // request with a JSON parse error on ill-formed UTF-8.
    result.stdout_data = utf8::ConsoleToUtf8(result.stdout_data);
    result.stderr_data = utf8::ConsoleToUtf8(result.stderr_data);

    if (result.timed_out)
    {
        console::write_line(fmt.Format("Command timed out after %? s and was terminated.", timeout_seconds), console::TextOrigin::error);
    }
    else if (result.exit_code != 0)
    {
        console::write_line(fmt.Format("Command finished with exit code %?", result.exit_code), console::TextOrigin::error);
    }
    else
    {
        console::write_line(fmt.Format("Command finished successfully in %? ms.", duration_ms), console::TextOrigin::tools);
    }

    std::ostringstream json;
    json << "{ \"status\": \"" << (result.timed_out ? "timeout" : "success") << "\", "
        << "\"command\": " << GetEscapedJSONString(command) << ", "
        << "\"cwd\": " << GetEscapedJSONString(cwd) << ", "
        << "\"exit_code\": " << result.exit_code << ", "
        << "\"timed_out\": " << (result.timed_out ? "true" : "false") << ", "
        << "\"truncated\": " << (result.truncated ? "true" : "false") << ", "
        << "\"duration_ms\": " << duration_ms << ", "
        << "\"stdout\": " << GetEscapedJSONString(result.stdout_data) << ", "
        << "\"stderr\": " << GetEscapedJSONString(result.stderr_data);

    if (result.timed_out)
        json << ", \"message\": \"Command exceeded timeout and was terminated\"";

    json << " }";

    return json.str();
}

#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <thread>
#include <atomic>

namespace {

struct ReadPipeArgs
{
    HANDLE pipe = nullptr;
    HANDLE process = nullptr;
    std::string *dest = nullptr;
    size_t max_bytes = 0;
    bool *truncated = nullptr;
};

void ReadPipeThread(ReadPipeArgs args)
{
    char buffer[4096];
    bool process_ended = false;
    int grace_ticks = 0;

    for (;;)
    {
        DWORD available = 0;

        if (!PeekNamedPipe(args.pipe, nullptr, 0, nullptr, &available, nullptr))
            break; // pipe is broken: the child side was closed

        if (available > 0)
        {
            DWORD to_read = static_cast<DWORD>(std::min<size_t>(available, sizeof(buffer)));
            DWORD read_bytes = 0;

            if (!ReadFile(args.pipe, buffer, to_read, &read_bytes, nullptr) || read_bytes == 0)
                break;

            AppendCapped(*args.dest, buffer, static_cast<size_t>(read_bytes), args.max_bytes, *args.truncated);
            continue;
        }

        if (process_ended)
        {
            // Drain remaining output for a bounded time after the main process exit,
            // then give up (handles can be kept open by processes spawned by the child)
            if (++grace_ticks > 200)
                break;
        }
        else if (WaitForSingleObject(args.process, 10) == WAIT_OBJECT_0)
        {
            process_ended = true;
            grace_ticks = 0;
        }
    }
}

} // namespace

bool RunCommandTool::ExecuteProcess(const std::string &command,
    const std::string &cwd,
    const std::string &stdin_data,
    size_t max_output_bytes,
    int timeout_seconds,
    ProcessResult &result) const
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    HANDLE stderr_read = nullptr;
    HANDLE stderr_write = nullptr;
    HANDLE stdin_read = nullptr;
    HANDLE stdin_write = nullptr;
    HANDLE nul_file = nullptr;
    PROCESS_INFORMATION pi{};
    std::thread stdout_thread, stderr_thread, stdin_thread;
    bool success = false;
    bool stdout_truncated = false;
    bool stderr_truncated = false;
    DWORD exit_code = static_cast<DWORD>(-1);

    auto close_handle = [](HANDLE &handle)
    {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
            handle = nullptr;
        }
    };

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0))
        goto finish;

    if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0))
        goto finish;

    if (!stdin_data.empty())
    {
        if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0))
            goto finish;
    }
    else
    {
        nul_file = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa, OPEN_EXISTING, 0, nullptr);

        if (nul_file == INVALID_HANDLE_VALUE)
            nul_file = nullptr;

        if (nul_file == nullptr)
            goto finish;
    }

    // Read ends must not be inherited by the child, otherwise EOF is never reported
    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0))
        goto finish;

    if (stdin_write != nullptr &&
        !SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0))
        goto finish;

    {
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = (stdin_read != nullptr) ? stdin_read : nul_file;
        si.hStdOutput = stdout_write;
        si.hStdError = stderr_write;

        std::string cmd_line = "cmd.exe /d /s /c \"" + command + "\"";

        if (!CreateProcessA(nullptr, &cmd_line[0], nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, cwd.c_str(), &si, &pi))
            goto finish;
    }

    // Close parent copies of the child-side handles so EOF reaches the readers
    close_handle(stdout_write);
    close_handle(stderr_write);
    close_handle(stdin_read);
    close_handle(nul_file);

    stdout_thread = std::thread(ReadPipeThread, ReadPipeArgs{ stdout_read, pi.hProcess, &result.stdout_data, max_output_bytes, &stdout_truncated });
    stderr_thread = std::thread(ReadPipeThread, ReadPipeArgs{ stderr_read, pi.hProcess, &result.stderr_data, max_output_bytes, &stderr_truncated });

    if (stdin_write != nullptr)
    {
        HANDLE stdin_write_local = stdin_write;
        stdin_thread = std::thread([&stdin_data, stdin_write_local]()
        {
            const char *data = stdin_data.data();
            size_t remaining = stdin_data.size();
            DWORD written = 0;

            while (remaining > 0)
            {
                DWORD chunk = static_cast<DWORD>(std::min<size_t>(remaining, 65536));

                if (!WriteFile(stdin_write_local, data, chunk, &written, nullptr) || written == 0)
                    break;

                data += written;
                remaining -= written;
            }

            CloseHandle(stdin_write_local);
        });
    }

    {
        DWORD wait_result = WaitForSingleObject(pi.hProcess,
            static_cast<DWORD>(timeout_seconds) * 1000);

        if (wait_result == WAIT_TIMEOUT)
        {
            result.timed_out = true;
            TerminateProcess(pi.hProcess, static_cast<UINT>(-1));
            WaitForSingleObject(pi.hProcess, 5000);
        }
    }

    if (stdin_thread.joinable())
        stdin_thread.join();

    stdout_thread.join();
    stderr_thread.join();

    GetExitCodeProcess(pi.hProcess, &exit_code);

    result.exit_code = static_cast<int>(exit_code);
    result.truncated = stdout_truncated || stderr_truncated;
    success = true;

finish:
    if (stdin_thread.joinable())
        stdin_thread.join();

    if (stdout_thread.joinable())
        stdout_thread.join();

    if (stderr_thread.joinable())
        stderr_thread.join();

    close_handle(stdout_read);
    close_handle(stdout_write);
    close_handle(stderr_read);
    close_handle(stderr_write);
    close_handle(stdin_read);
    close_handle(stdin_write);
    close_handle(nul_file);

    if (pi.hProcess != nullptr)
    {
        if (!success)
            TerminateProcess(pi.hProcess, static_cast<UINT>(-1));

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        pi.hProcess = nullptr;
        pi.hThread = nullptr;
    }

    return success;
}

bool RunCommandTool::ConfirmOperation(const std::string &command) const
{
    Formatter fmt;
    auto it = m_safe_commands.find(command);
    if (it == m_safe_commands.end())
    {
        console::write_line("Warning: command execution.", console::TextOrigin::tools);
        console::write_line(fmt.Format("Command: %?", command), console::TextOrigin::tools);
        console::write_line("Do you want to proceed? (y/n): ", console::TextOrigin::tools);

        std::string response;
        std::getline(std::cin, response);

        if (response != "y" && response != "Y")
        {
            console::write_line("Operation cancelled by user.", console::TextOrigin::tools);
            return false;
        }
        else
        {
            console::write_line("Operation confirmed by user.", console::TextOrigin::tools);
            return true;
        }
    }
}

#else

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <ctime>

bool RunCommandTool::ExecuteProcess(const std::string &command,
    const std::string &cwd,
    const std::string &stdin_data,
    size_t max_output_bytes,
    int timeout_seconds,
    ProcessResult &result) const
{
    const bool use_stdin = !stdin_data.empty();

    int out_pipe[2] = { -1, -1 };
    int err_pipe[2] = { -1, -1 };
    int in_pipe[2] = { -1, -1 };

    void (*old_sigpipe)(int) = signal(SIGPIPE, SIG_IGN);

    auto close_fd = [](int &fd)
    {
        if (fd >= 0)
        {
            close(fd);
            fd = -1;
        }
    };

    auto fail = [&]() -> bool
    {
        close_fd(out_pipe[0]); close_fd(out_pipe[1]);
        close_fd(err_pipe[0]); close_fd(err_pipe[1]);
        close_fd(in_pipe[0]);  close_fd(in_pipe[1]);
        signal(SIGPIPE, old_sigpipe);
        return false;
    };

    if (pipe(out_pipe) != 0)
        return fail();

    if (pipe(err_pipe) != 0)
        return fail();

    if (use_stdin && pipe(in_pipe) != 0)
        return fail();

    pid_t pid = fork();

    if (pid < 0)
        return fail();

    if (pid == 0)
    {
        if (use_stdin)
        {
            dup2(in_pipe[0], STDIN_FILENO);
        }
        else
        {
            int nul = open("/dev/null", O_RDONLY);

            if (nul >= 0)
            {
                dup2(nul, STDIN_FILENO);
                close(nul);
            }
        }

        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);

        close_fd(out_pipe[0]); close_fd(out_pipe[1]);
        close_fd(err_pipe[0]); close_fd(err_pipe[1]);
        close_fd(in_pipe[0]);  close_fd(in_pipe[1]);

        if (chdir(cwd.c_str()) != 0)
            _exit(126);

        execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    }

    close_fd(out_pipe[1]);
    close_fd(err_pipe[1]);
    if (use_stdin)
        close_fd(in_pipe[0]);

    fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);
    if (use_stdin)
        fcntl(in_pipe[1], F_SETFL, O_NONBLOCK);

    bool stdout_truncated = false;
    bool stderr_truncated = false;
    bool out_open = true;
    bool err_open = true;
    bool in_open = use_stdin;
    size_t in_pos = 0;
    bool timed_out = false;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);

    while (out_open || err_open || in_open)
    {
        long long remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();

        if (remaining_ms <= 0)
        {
            timed_out = true;
            break;
        }

        struct pollfd fds[3];
        int count = 0;
        int out_index = -1;
        int err_index = -1;
        int in_index = -1;

        if (out_open)
        {
            out_index = count;
            fds[count].fd = out_pipe[0];
            fds[count].events = POLLIN;
            fds[count].revents = 0;
            count++;
        }

        if (err_open)
        {
            err_index = count;
            fds[count].fd = err_pipe[0];
            fds[count].events = POLLIN;
            fds[count].revents = 0;
            count++;
        }

        if (in_open)
        {
            in_index = count;
            fds[count].fd = in_pipe[1];
            fds[count].events = POLLOUT;
            fds[count].revents = 0;
            count++;
        }

        int ready = poll(fds, static_cast<nfds_t>(count), static_cast<int>(remaining_ms));

        if (ready < 0)
        {
            if (errno == EINTR)
                continue;

            break;
        }

        if (ready == 0)
        {
            timed_out = true;
            break;
        }

        if (out_index >= 0 && (fds[out_index].revents & (POLLIN | POLLHUP | POLLERR)) != 0)
        {
            char buffer[4096];

            for (;;)
            {
                ssize_t r = read(out_pipe[0], buffer, sizeof(buffer));

                if (r > 0)
                {
                    AppendCapped(result.stdout_data, buffer, static_cast<size_t>(r), max_output_bytes, stdout_truncated);
                    continue;
                }

                if (r == 0)
                {
                    close_fd(out_pipe[0]);
                    out_open = false;
                    break;
                }

                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                    break;

                close_fd(out_pipe[0]);
                out_open = false;
                break;
            }
        }

        if (err_index >= 0 && (fds[err_index].revents & (POLLIN | POLLHUP | POLLERR)) != 0)
        {
            char buffer[4096];

            for (;;)
            {
                ssize_t r = read(err_pipe[0], buffer, sizeof(buffer));

                if (r > 0)
                {
                    AppendCapped(result.stderr_data, buffer, static_cast<size_t>(r), max_output_bytes, stderr_truncated);
                    continue;
                }

                if (r == 0)
                {
                    close_fd(err_pipe[0]);
                    err_open = false;
                    break;
                }

                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                    break;

                close_fd(err_pipe[0]);
                err_open = false;
                break;
            }
        }

        if (in_open && in_index >= 0 && (fds[in_index].revents & (POLLOUT | POLLERR | POLLHUP)) != 0)
        {
            while (in_pos < stdin_data.size())
            {
                size_t chunk = std::min<size_t>(stdin_data.size() - in_pos, 65536);
                ssize_t w = write(in_pipe[1], stdin_data.data() + in_pos, chunk);

                if (w > 0)
                {
                    in_pos += static_cast<size_t>(w);
                    continue;
                }

                if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                    break;

                close_fd(in_pipe[1]);
                in_open = false;
                break;
            }

            if (in_open && in_pos >= stdin_data.size())
            {
                close_fd(in_pipe[1]);
                in_open = false;
            }
        }
    }

    close_fd(out_pipe[0]);
    close_fd(err_pipe[0]);
    close_fd(in_pipe[1]);

    int status = 0;

    if (timed_out)
    {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    else
    {
        bool exited = false;

        for (int i = 0; i < 100 && !exited; ++i)
        {
            pid_t r = waitpid(pid, &status, WNOHANG);

            if (r == pid)
            {
                exited = true;
                break;
            }

            if (r < 0)
                break;

            timespec ts{ 0, 10 * 1000 * 1000 };
            nanosleep(&ts, nullptr);
        }

        if (!exited)
        {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
        }
    }

    if (WIFEXITED(status))
        result.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
        result.exit_code = 128 + WTERMSIG(status);

    result.timed_out = timed_out;
    result.truncated = stdout_truncated || stderr_truncated;

    signal(SIGPIPE, old_sigpipe);
    return true;
}

#endif // _WIN32
