#ifndef COMMAND_CHANGE_RULES_INCLUDED_H
#define COMMAND_CHANGE_RULES_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"
#include "format_util.h"
#include "working_dir.h"
#include <iostream>
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>

static int getch()
{
    termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    int ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}
#endif

/// Selects or shows existing agent rules (system prompt) from Psittacula/settings
class CommandChangeRules : public ChatCommand
{
    public:
        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {
            if (!args.empty() && args[0] == "show")
            {
                ShowCurrentAgentRules(client);
                return;
            }
            ChooseRules(client);
        }

        std::string Description() override
        {
            return "Choose rules (system prompt) from /settings directory.";
        }

    private:
        Formatter formatter;

        void ShowCurrentAgentRules(std::unique_ptr<AiClient> &client)
        {
            ActivateAlternateScreen();
            
            std::cout << "\x1b[2J\x1b[H";
            console::write_line("====================================================", TextOrigin::filesystem);
            console::write_line("================= Current Agent Rules ===============", TextOrigin::filesystem);
            console::write_line("====================================================", TextOrigin::filesystem);
            
            std::string rules = client->GetAgentRules();
            console::write_line(rules);
            
            console::write_line("\n\nPress any key to continue...");
            
            std::cin.get();

            RestoreMainScreen();
        }

        void ChooseRules(std::unique_ptr<AiClient> &client)
        {
            ActivateAlternateScreen();

            std::string settings_dir = WorkingDir::GetInstance().GetSettingsDir();
            namespace fs = std::filesystem;

            std::vector<std::string> agent_rules;

            for (auto &entry : fs::directory_iterator(settings_dir))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".txt")
                    agent_rules.push_back(entry.path().string());
            }

            if (agent_rules.empty())
            {
                console::write_line("No agent rules files found in /settings.", TextOrigin::error);
                RestoreMainScreen();
                return;
            }

            int index = 0;
            DrawMenu(agent_rules, index);

            while (true)
            {
#ifdef _WIN32
                int c = _getch();
                if (c == 224)
                {
                    int arrow = _getch();
                    if (arrow == 72 && index > 0) index--;
                    else if (arrow == 80 && index < agent_rules.size() - 1) index++;
                    DrawMenu(agent_rules, index);
                }
                else if (c == 13)
                {
                    break;
                }
#else
                int c = getch();
                if (c == '\x1b')
                {
                    int c1 = getch();
                    if (c1 == '[')
                    {
                        int c2 = getch();
                        if (c2 == 'A' && index > 0) index--;
                        else if (c2 == 'B' && index < agent_rules.size() - 1) index++;
                        DrawMenu(agent_rules, index);
                    }
                }
                else if (c == '\n' || c == '\r')
                {
                    break;
                }
#endif
            }

            // Load selected agent file
            std::string selected_file = agent_rules[index];
            std::ifstream in(selected_file);

            if (!in.is_open())
            {
                console::write_line("Error: Could not open agent rules file.", TextOrigin::error);
                RestoreMainScreen();
                return;
            }

            std::stringstream buffer;
            buffer << in.rdbuf();
            std::string agent_prompt = buffer.str();

            // Apply agent to client
            client->SetAgentRules(agent_prompt);

            RestoreMainScreen();
            console::write_line("Agent rules loaded: " + selected_file, TextOrigin::filesystem);
        }

        void DrawMenu(const std::vector<std::string> &agents, int index)
        {
            std::cout << "\x1b[2J\x1b[H";
            console::write_line("====================================================", TextOrigin::filesystem);
            console::write_line("============ Choose agent rules to load ==============", TextOrigin::filesystem);
            console::write_line("====================================================", TextOrigin::filesystem);
            console::write_line(formatter.Format("Found agent rules: %?", agents.size()), TextOrigin::filesystem);
            console::write_line("Use Up/Down to choose, Enter to select.\n");

            for (int i = 0; i < agents.size(); ++i)
            {
                if (i == index)
                    std::cout << "\x1b[7m";

                std::cout << (i + 1) << " - " << agents[i] << "\x1b[0m\n";
            }
        }
};

#endif // COMMAND_CHANGE_RULES_INCLUDED_H