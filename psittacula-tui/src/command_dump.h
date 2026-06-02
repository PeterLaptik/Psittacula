#ifndef COMMAND_DUMP_INCLUDED_H
#define COMMAND_DUMP_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"
#include "working_dir.h"
#include <string>
#include <vector>
#include <fstream>
#include <chrono>

class CommandDump : public ChatCommand
{
public:
    void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
    {
        std::string project_dir = WorkingDir::GetInstance().GetProjectDir();
        std::string body = client->GetDialogueBody();

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
        std::string file_path = project_dir + "/dialogue_" + timestamp + ".txt";

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

        console::write_line("Dialogue body saved to: " + file_path, TextOrigin::filesystem);
    }

    std::string Description() override
    {
        return "Saves dialogue full body as a text file.";
    }
};

#endif // COMMAND_DUMP_INCLUDED_H
