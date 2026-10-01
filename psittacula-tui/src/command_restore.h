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
        using ChatCommand::ChatCommand;

        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {
            if (!client.get())
            {
                console::write_line("Error: AiClient is not initialized / no connection to LLM", TextOrigin::error);
                return;
            }

            if (args.empty()) {
                console::write_line("Error: No file path provided. Usage: restore <filename>", TextOrigin::error);
                return;
            }

            std::string file_name = args[0];

            // Add .txt extension, if the file name does not have it
            if (file_name.size() < 4 || file_name.compare(file_name.size() - 4, 4, ".txt") != 0)
                file_name += ".txt";

            std::string log_dir = WorkingDir::GetInstance().GetLogsDir();
            std::string file_path = log_dir + "/" + file_name;

            // Read file contents
            std::ifstream in(file_path, std::ios::in);
            if (!in)
            {
                console::write_line("Failed to open file in logs dir for reading: " + file_path,
                    TextOrigin::error);
                return;
            }

            std::string content((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
            in.close();

            CommandCleanContext cmd_clean{m_app};
            cmd_clean.Execute(client, {});

            // Restore dialogue from file contents
            client->RestoreDialogueFrom(content);

            console::write_line("Dialogue restored from: " + file_path, TextOrigin::tools);
        }

        std::string Description() override
        {
            return "Restores dialogue from a text file (from a project/logs directory). \n\t[ARG] filename.";
        }
};

#endif // COMMAND_RESTORE_INCLUDED_H