#ifndef COMMAND_RESTORE_INCLUDED_H
#define COMMAND_RESTORE_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"
#include "working_dir.h"
#include <string>
#include <vector>
#include <fstream>

/// Restores dialogue from a text file (from a project directory)
class CommandRestore : public ChatCommand
{
    public:
        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {
            if (args.empty()) {
                console::write_line("Error: No file path provided. Usage: restore <filename>", TextOrigin::error);
                return;
            }

            std::string project_dir = WorkingDir::GetInstance().GetLogsDir();
            std::string file_path = project_dir + "/" + args[0];

            // Read file contents
            std::ifstream in(file_path, std::ios::in);
            if (!in)
            {
                console::write_line("Failed to open file for reading: " + file_path,
                    TextOrigin::error);
                return;
            }

            std::string content((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
            in.close();

            // Restore dialogue from file contents
            client->RestoreDialogueFrom(content);

            console::write_line("Dialogue restored from: " + file_path, TextOrigin::filesystem);
        }

        std::string Description() override
        {
            return "Restores dialogue from a text file (from a project/logs directory). \n\t[ARG] filename.";
        }
};

#endif // COMMAND_RESTORE_INCLUDED_H