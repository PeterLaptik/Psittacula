#ifndef COMMAND_DUMP_TEXT_INCLUDED_H
#define COMMAND_DUMP_TEXT_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"
#include "working_dir.h"
#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <filesystem>

/// Saves dialogue full JSON text as a markdown text file (to a project directory)
/// Can be useful for debug
class CommandDumpText : public ChatCommand
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

            std::string dir_to_save = WorkingDir::GetInstance().GetLogsDir();
            std::string body = client->GetDialogueText();

            std::string file_path;
            if (!args.empty()) {
                namespace fs = std::filesystem;

                fs::path arg_path(args[0]);

                if (arg_path.has_parent_path())
                {
                    // Argument contains a path: save into the full path provided
                    file_path = arg_path.string();
                    if (!arg_path.has_extension())
                        file_path += ".md";

                    // Make sure the target directory exists
                    std::error_code ec;
                    fs::create_directories(fs::path(file_path).parent_path(), ec);
                }
                else
                {
                    // Argument is a bare filename: save into the logs directory
                    file_path = dir_to_save + "/" + arg_path.string();
                    if (!arg_path.has_extension())
                        file_path += ".md";
                }
            } else {
                // Generate timestamp
                auto now = std::chrono::system_clock::now();
                std::time_t t = std::chrono::system_clock::to_time_t(now);

                std::tm tm{};
#ifdef _WIN32
                localtime_s(&tm, &t);
#else
                localtime_r(&t, &tm);
#endif

                char timestamp[32];
                std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", &tm);

                // Build filename
                file_path = dir_to_save + "/dialogue_text_" + timestamp + ".md";
            }

            // Write file
            std::ofstream out(file_path, std::ios::out | std::ios::trunc);
            if (!out)
            {
                console::write_line("Failed to open file for writing: " + file_path,
                    TextOrigin::error);
                return;
            }

            out << body;
            out.close();

            console::write_line("Dialogue text saved to: " + file_path, TextOrigin::tools);
        }

        std::string Description() override
        {
            return "Saves dialogue full JSON text as a text file. \n\t[ARG] filename (saved to logs dir) or full path (saved as-is).";
        }
};

#endif // COMMAND_DUMP_TEXT_INCLUDED_H
