#ifndef COMMAND_DUMP_TEXT_INCLUDED_H
#define COMMAND_DUMP_TEXT_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"
#include "working_dir.h"
#include <string>
#include <vector>
#include <fstream>
#include <chrono>

/// Saves dialogue full JSON text as a text file (to a project directory)
/// Can be useful for debug
class CommandDumpText : public ChatCommand
{
    public:
        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {
            std::string dir_to_save = WorkingDir::GetInstance().GetLogsDir();
            std::string body = client->GetDialogueText();

            std::string file_path;
            if (!args.empty()) {
                // Use provided filename
                file_path = dir_to_save + "/" + args[0] + ".md";
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

            console::write_line("Dialogue text saved to: " + file_path, TextOrigin::filesystem);
        }

        std::string Description() override
        {
            return "Saves dialogue full JSON text as a text file (to a project directory). \n\t[ARG] filename.";
        }
};

#endif // COMMAND_DUMP_TEXT_INCLUDED_H
