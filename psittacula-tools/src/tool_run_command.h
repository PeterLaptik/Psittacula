#ifndef TOOL_RUN_COMMAND_INCLUDED_H
#define TOOL_RUN_COMMAND_INCLUDED_H

#include "tool_base.h"
#include <string>
#include <vector>
#include <set>

class RunCommandTool : public ToolBase
{
    public:
        RunCommandTool() = default;
        ~RunCommandTool() override = default;

        ToolBase *Clone() override { return new RunCommandTool(); }

        std::string Execute(std::vector<ToolParameter> &params_values) override;

        void Undo() override {}
        void Redo() override {}

        std::string GetToolName() const override { return "run_command"; }
        std::string GetToolDescription() const override { return "Executes a shell command (cmd.exe on Windows, /bin/sh on POSIX) inside the project working directory and returns its exit code, stdout and stderr. Use for builds, tests, git and other command-line operations. Commands run with a timeout and are not otherwise restricted, except: on POSIX the child runs in an isolated session with a private pseudo-terminal (it can never reach the user's terminal, password prompters simply fail), and any interactive/keep_tty request is refused - the command must be non-interactive (pipe secrets via stdin_data)."; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

        bool CanBeReverted() override { return false; }

        // Plain result data: public so the regression suite can name it when it
        // drives ExecuteProcess() directly through the friend seam below
        struct ProcessResult
        {
            int exit_code = -1;
            bool timed_out = false;
            bool truncated = false;
            // POSIX only: true when the child was moved into its own session with
            // a private pty as the controlling terminal, so it could not reach
            // the user's terminal (password prompters open /dev/tty directly)
            bool sandboxed = false;
            std::string stdout_data;
            std::string stderr_data;
        };

    private:
        // Test seam: the regression suite exercises the real process plumbing directly 
        // (Execute() would block on the interactive confirm dialogue)
        friend int RunCommandSandboxTests();

        bool ExecuteProcess(const std::string &command,
            const std::string &cwd,
            const std::string &stdin_data,
            bool child_needs_tty,
            size_t max_output_bytes,
            int timeout_seconds,
            ProcessResult &result) const;
};

#endif // TOOL_RUN_COMMAND_INCLUDED_H
