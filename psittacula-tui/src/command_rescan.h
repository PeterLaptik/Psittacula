#ifndef COMMAND_RESCAN_INCLUDED_H
#define COMMAND_RESCAN_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"
#include "working_dir.h"
#include <string>

/// Re-sets the project directory to the same value.
/// The side effect: WorkingDir::SetProjectDir() clears and rebuilds the project file list, i.e. rescans files in the directory.
class CommandRescan : public ChatCommand
{
    public:
        using ChatCommand::ChatCommand;

        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {
            // The command ignores arguments and asks nothing.
            (void)client;
            (void)args;

            const std::string project_dir = WorkingDir::GetInstance().GetProjectDir();

            // Setting the same path clears and rescans the project files list.
            WorkingDir::GetInstance().SetProjectDir(project_dir);

            const size_t count = WorkingDir::GetInstance().GetProjectFilesList().size();

            console::write_line("\nRescanned project directory:", TextOrigin::tools);
            console::write_line("  " + project_dir, TextOrigin::tools);
            console::write_line("Files found: " + std::to_string(count), TextOrigin::tools);
        }

        std::string Description() override
        {
            return "Rescan the project directory (re-read the list of project files).";
        }
};

#endif // COMMAND_RESCAN_INCLUDED_H
