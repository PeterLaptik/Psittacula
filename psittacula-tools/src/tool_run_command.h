#ifndef TOOL_RUN_COMMAND_INCLUDED_H
#define TOOL_RUN_COMMAND_INCLUDED_H

#include "tool_base.h"
#include <string>
#include <vector>

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
        std::string GetToolDescription() const override { return "Executes a shell command (cmd.exe on Windows, /bin/sh on POSIX) inside the project working directory and returns its exit code, stdout and stderr. Use for builds, tests, git and other command-line operations. Commands run with a timeout and are not otherwise restricted."; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

        bool CanBeReverted() override { return false; }

    private:
        struct ProcessResult
        {
            int exit_code = -1;
            bool timed_out = false;
            bool truncated = false;
            std::string stdout_data;
            std::string stderr_data;
        };

        bool ExecuteProcess(const std::string &command,
            const std::string &cwd,
            const std::string &stdin_data,
            size_t max_output_bytes,
            int timeout_seconds,
            ProcessResult &result) const;
};

#endif // TOOL_RUN_COMMAND_INCLUDED_H
