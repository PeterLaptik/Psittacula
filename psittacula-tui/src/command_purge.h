#ifndef COMMAND_PURGE_INCLUDED_H
#define COMMAND_PURGE_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"
#include "working_dir.h"
#include <string>
#include <vector>
#include <filesystem>
#include <system_error>

/// Cleans up a project logs directory where dialogue dumps are being kept
/// Can be useful for debug / housekeeping
class CommandPurge : public ChatCommand
{
    public:
        using ChatCommand::ChatCommand;

        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {
            if (!client.get())
            {
                console::write_line("Error: AI client is not initialized / no connection", console::TextOrigin::error);
                return;
            }

            namespace fs = std::filesystem;

            std::string log_dir = WorkingDir::GetInstance().GetLogsDir();

            if (!fs::exists(log_dir))
            {
                console::write_line("Logs directory does not exist, nothing to purge: " + log_dir,
                    TextOrigin::tools);
                return;
            }

            std::error_code ec;

            // Purge a single dump file, if a filename was provided
            if (!args.empty())
            {
                for (const std::string &arg : args)
                {
                    std::string file_name = arg;

                    // Add .txt extension, if the file name does not have it
                    if (file_name.size() < 4 || file_name.compare(file_name.size() - 4, 4, ".txt") != 0)
                        file_name += ".txt";

                    std::string file_path = log_dir + "/" + file_name;

                    if (!fs::exists(file_path))
                    {
                        console::write_line("File not found in logs dir, skipping: " + file_path,
                            TextOrigin::tools);
                        continue;
                    }

                    ec.clear();
                    fs::remove(file_path, ec);

                    if (ec)
                    {
                        console::write_line("Failed to remove file: " + file_path + " (" + ec.message() + ")",
                            TextOrigin::error);
                        continue;
                    }

                    console::write_line("File removed: " + file_path, TextOrigin::tools);
                }

                return;
            }

            // Purge the whole logs directory contents
            const std::uintmax_t removed = fs::remove_all(log_dir, ec);

            // Recreate the (now empty) logs directory
            std::error_code create_ec;
            fs::create_directories(log_dir, create_ec);

            if (ec || create_ec)
            {
                std::string reason = ec ? ec.message() : create_ec.message();
                console::write_line("Failed to purge logs directory: " + log_dir + " (" + reason + ")",
                    TextOrigin::error);
                return;
            }

            console::write_line("Purged " + std::to_string(removed) + " entries from: " + log_dir,
                TextOrigin::tools);
        }

        std::string Description() override
        {
            return "Cleans up a project logs directory (deletes all dump files). \n\t[ARG] filename to purge a single dump.";
        }
};

#endif // COMMAND_PURGE_INCLUDED_H
