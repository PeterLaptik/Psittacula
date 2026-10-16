// Regression tests for the run_command POSIX terminal sandbox.
//
// Bug being pinned: a child that prompted for a password (sudo, ssh, mysql -
// they open /dev/tty, bypassing all redirected stdio) used to latch onto
// Psittacula's raw-mode terminal: it drained the user's keystrokes, could not
// accept input anyway, and the parent's poll loop carried on until the whole
// timeout - the screen was stuck on "Password:" with a dead cursor.
//
// The fix: ExecuteProcess gives the child its own session (setsid) with a
// private pty as the controlling terminal, so /dev/tty resolves inside the
// sandbox and dies with it. The tests spawn commands via the tool's own
// ExecuteProcess (not via Execute: it would block on the interactive confirm
// dialogue) and assert the invariants below.
//
// The whole file is POSIX-only; on Windows no sandbox exists (children cannot
// reach the console there), so the cases self-report as passing.
#include "test_framework.h"
#include "tool_run_command.h"

#include <string>
#include <vector>

#ifdef _WIN32
int RunCommandSandboxTests()
{
    return 0; // Windows: nothing to test, the POSIX sandbox does not apply
}
#else

#include <cstdio>
#include <unistd.h>
#include <chrono>

namespace
{
    // cwd is not validated by ExecuteProcess (only Execute validates it),
    // so the current directory of the test process is fine
    const char *kCwd = ".";

    // Controlling terminal of the CALLING process ("pts/N" / "ttysN" / "?" / "")
    std::string OwnTtyName()
    {
        std::string cmd = std::string("ps -o tty= -p ") + std::to_string(static_cast<long>(getpid()));

        FILE *pp = popen(cmd.c_str(), "r");

        if (!pp)
            return "";

        std::string line;
        char buffer[256];

        while (fgets(buffer, sizeof(buffer), pp))
            line += buffer;

        pclose(pp);

        while (!line.empty() && (line.back() == '\n' || line.back() == '\r' ||
            line.back() == ' ' || line.back() == '\t'))
            line.pop_back();

        return line;
    }

    bool Contains(const std::string &haystack, const char *needle)
    {
        return haystack.find(needle) != std::string::npos;
    }
}

int RunCommandSandboxTests()
{
    int failed = 0;

    // Drives the tool through the friend seam. The call MUST stay lexically
    // inside RunCommandSandboxTests(): friendship is lexical, not transitive,
    // so a helper in the anonymous namespace has no access ([class.access]/6).
    // RunTheTool is a LOCAL class-type callable defined in this function body:
    // members of a class local to F are compiled as if defined within F and
    // inherit the enclosing function's access rights.
    struct RunTheToolFn
    {
        RunCommandTool::ProcessResult operator()(const std::string &command,
            int timeout_seconds,
            const std::string &stdin_data = "") const
        {
            RunCommandTool tool;
            RunCommandTool::ProcessResult result;
            tool.ExecuteProcess(command, kCwd, stdin_data, false, 65536, timeout_seconds, result);
            return result;
        }
    };

    const RunTheToolFn RunTheTool = {};

    {
        bool all_ok = true;

        // --- Case 1: the POSIX branch reports the sandbox in the result ---
        {
            auto result = RunTheTool("true", 30);
            all_ok = all_ok && result.sandboxed && result.exit_code == 0;
            if (!(result.sandboxed && result.exit_code == 0))
            {
                std::printf("    FAIL %s:%d  sandboxed/exit_code (sandboxed=%d, exit=%d)\n",
                    "test_run_command_sandbox.cpp", __LINE__,
                    result.sandboxed ? 1 : 0, result.exit_code);
            }
        }

        // --- Case 2: regular commands stay fully functional ---
        {
            auto result = RunTheTool("printf 'ok-from-sh'", 30);
            all_ok = all_ok && result.exit_code == 0 && result.stdout_data == "ok-from-sh";
            if (!(result.exit_code == 0 && result.stdout_data == "ok-from-sh"))
            {
                std::printf("    FAIL %s:%d  regular command broken (exit=%d, stdout='%s')\n",
                    "test_run_command_sandbox.cpp", __LINE__,
                    result.exit_code, result.stdout_data.c_str());
            }
        }

        // --- Case 3: stdin data still flows to the child ---
        {
            auto result = RunTheTool("cat", 30, "piped-secret\n");
            all_ok = all_ok && result.exit_code == 0 && result.stdout_data == "piped-secret\n";
            if (!(result.exit_code == 0 && result.stdout_data == "piped-secret\n"))
            {
                std::printf("    FAIL %s:%d  stdin_data broken (exit=%d, stdout='%s')\n",
                    "test_run_command_sandbox.cpp", __LINE__,
                    result.exit_code, result.stdout_data.c_str());
            }
        }

        // --- Case 4: timeout still terminates a stubborn child ---
        {
            const auto started = std::chrono::steady_clock::now();
            auto result = RunTheTool("sleep 30", 2);
            const long long elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - started).count();

            all_ok = all_ok && result.timed_out && elapsed < 10;
            if (!(result.timed_out && elapsed < 10))
            {
                std::printf("    FAIL %s:%d  timeout must kill the child (timed_out=%d, elapsed=%llds)\n",
                    "test_run_command_sandbox.cpp", __LINE__,
                    result.timed_out ? 1 : 0, elapsed);
            }
        }

        // --- Case 5: writing to "the terminal" is captured, not lost ---
        {
            auto result = RunTheTool("echo secret-prompt-text > /dev/tty", 30);
            all_ok = all_ok && Contains(result.stderr_data, "secret-prompt-text");
            if (!Contains(result.stderr_data, "secret-prompt-text"))
            {
                std::printf("    FAIL %s:%d  tty output must be captured in stderr (stderr='%s')\n",
                    "test_run_command_sandbox.cpp", __LINE__, result.stderr_data.c_str());
            }
        }

        // --- Case 6: a tty-probing utility (tcgetattr / TIOCGWINSZ) does not hang ---
        {
            auto result = RunTheTool("stty size < /dev/tty", 30);
            all_ok = all_ok && !result.timed_out;
            if (result.timed_out)
            {
                std::printf("    FAIL %s:%d  stty against the sandbox pty must not hang\n",
                    "test_run_command_sandbox.cpp", __LINE__);
            }
        }

        // --- Case 7 (THE regression pin): the child's controlling terminal is not ours ---
        {
            const std::string parent_tty = OwnTtyName();

            // Without a parent tty there is nothing to discriminate against
            if (!parent_tty.empty() && parent_tty != "?")
            {
                auto result = RunTheTool("echo CHILD_TTY=$(ps -o tty= -p $$ | tr -d ' \\t')", 30);
                const size_t pos = result.stdout_data.find("CHILD_TTY=");
                const std::string child_tty = (pos == std::string::npos) ? "" :
                    result.stdout_data.substr(pos + 10);

                // The child must see its own sandbox pty, never the terminal
                // Psittacula is driven from; equality means the fix regressed
                all_ok = all_ok && child_tty != parent_tty && !child_tty.empty();
                if (child_tty == parent_tty || child_tty.empty())
                {
                    std::printf("    FAIL %s:%d  child controlling tty mismatch (child='%s', parent='%s')\n",
                        "test_run_command_sandbox.cpp", __LINE__,
                        child_tty.c_str(), parent_tty.c_str());
                }            }
        }

        if (!all_ok)
            ++failed;
    }

    return failed;
}

#endif // _WIN32

// Registered like any other PS_TEST case, so it shows up in the default run
PS_TEST(run_command_posix_sandbox_invariants)
{
#ifdef _WIN32
    PS_CHECK_MSG(true, "Windows: the POSIX terminal sandbox does not apply here");
#else
    PS_CHECK_MSG(RunCommandSandboxTests() == 0, "run_command POSIX sandbox invariants failed");
#endif
}
