#ifndef COMMAND_CHANGE_PROJECT_DIR_INCLUDED_H
#define COMMAND_CHANGE_PROJECT_DIR_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"
#include "format_util.h"
#include "working_dir.h"
#include <iostream>
#include <filesystem>

/// Changes project directore / sandbox
class CommandChangeProjectDir : public ChatCommand
{
    public:
        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {
            ActivateAlternateScreen();

            namespace fs = std::filesystem;

            std::string current = WorkingDir::GetInstance().GetWorkDir();
            console::write_line("Current project directory:", TextOrigin::filesystem);
            console::write_line("  " + current + "\n");

            std::string new_path;

            // If there are arguments, take the first one as the new path
            if (!args.empty())
            {
                // Arguments preparation uses spliting by spaces.
                // Taking spaces into account: '/path/to/some foldef/abc' -> '/path/to/some', 'foldef/abc'
                std::ostringstream oss;
                for (size_t i = 0; i < args.size(); ++i)
                {
                    if (i > 0) oss << ' ';
                    oss << args[i];
                }
                new_path = oss.str();
            }
            else
            {
                std::cout << "Enter new project directory path: ";
                std::getline(std::cin, new_path);
            }

            if (new_path.empty())
            {
                console::write_line("No path entered. Aborting.\n", TextOrigin::error);
                RestoreMainScreen();
                return;
            }

            EnsureTrailingSeparator(new_path);

            fs::path p(new_path);

            if (!fs::exists(p))
            {
                console::write_line("Directory does not exist.", TextOrigin::error);

                std::cout << "Create it? (y/n): ";
                char c;
                std::cin >> c;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

                if (c == 'y' || c == 'Y')
                {
                    std::error_code ec;
                    fs::create_directories(p, ec);
                    if (ec)
                    {
                        console::write_line("Failed to create directory.\n", TextOrigin::error);
                        RestoreMainScreen();
                        return;
                    }
                    console::write_line("Directory created.\n", TextOrigin::filesystem);
                }
                else
                {
                    console::write_line("Aborted.\n", TextOrigin::error);
                    RestoreMainScreen();
                    return;
                }
            }

            WorkingDir::GetInstance().SetProjectDir(p.string());

            RestoreMainScreen();
            console::write_line("Project directory changed to:", TextOrigin::filesystem);
            console::write_line("  " + p.string() + "\n");
        }

        std::string Description() override
        {
            return "Change the project directory used by file tools.";
        }

    private:
        void EnsureTrailingSeparator(std::string &path)
        {
            if (path.empty())
                return;

            char sep = std::filesystem::path::preferred_separator;

            if (path.back() == '/' || path.back() == '\\')
                return;

            path += sep;
        }
};

#endif // COMMAND_CHANGE_PROJECT_INCLUDED_H