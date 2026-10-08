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
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

/// Selects or shows existing agent rules (system prompt) from Psittacula/settings
class CommandChangeRules : public ChatCommand
{
    public:
        using ChatCommand::ChatCommand;

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

        // Waits up to timeout_ms for the next byte on stdin (raw-mode menus).
        // Used to tell a bare Escape (quit) from the first byte of an escape sequence such as ESC [ A (navigation).
#ifndef _WIN32
        inline bool MenuInputReady(int timeout_ms)
        {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);

            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
        }

        // Unbuffered read(): stdio could swallow the second byte of an
        // escape sequence into its own buffer, hiding it from MenuInputReady
        int getch()
        {
            termios oldt, newt;
            tcgetattr(STDIN_FILENO, &oldt);
            newt = oldt;
            newt.c_lflag &= ~(ICANON | ECHO);
            newt.c_iflag &= ~(ICRNL | INLCR | IXON);
            newt.c_cc[VMIN] = 1;
            newt.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);

            unsigned char ch = 0;
            int result = read(STDIN_FILENO, &ch, 1);

            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            return result == 1 ? static_cast<int>(ch) : -1;
        }
#endif

        void ShowCurrentAgentRules(std::unique_ptr<AiClient> &client)
        {
            ActivateAlternateScreen();
            
            std::cout << "\x1b[2J\x1b[H";
            console::write_line("====================================================", TextOrigin::tools);
            console::write_line("================= Current Agent Rules ===============", TextOrigin::tools);
            console::write_line("====================================================", TextOrigin::tools);
            
            std::string rules = client->GetAgentRules();
            console::write_line(rules);
            
            console::write_line("\n\nPress any key to continue...");
            
            {
                console::LineInputScope line_input;
                std::cin.get();
            }

            RestoreMainScreen();
        }

        void ChooseRules(std::unique_ptr<AiClient> &client)
        {
            if (!client.get())
            {
                console::write_line("Error: AiClient is not initialized / no connection to LLM", TextOrigin::error);
                return;
            }

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
                else if (c == 27)
                {
                    // Escape quits without selecting
                    RestoreMainScreen();
                    return;
                }
                else if (c == 13)
                {
                    break;
                }
#else
                int c = getch();

                if (c == -1)
                {
                    // stdin failure / EOF: staying here is useless
                    RestoreMainScreen();
                    return;
                }

                if (c == '\x1b')
                {
                    // Bare ESC (no byte follows within 50 ms) quits the menu;
                    // ESC [ ... is a navigation sequence
                    if (!MenuInputReady(50))
                    {
                        RestoreMainScreen();
                        return;
                    }

                    int c1 = getch();
                    if (c1 == '[')
                    {
                        int c2 = getch();
                        if (c2 == 'A' && index > 0) index--;
                        else if (c2 == 'B' && index < agent_rules.size() - 1) index++;
                        DrawMenu(agent_rules, index);
                    }
                    // any other sequence: swallowed, menu stays
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
            console::write_line("Agent rules loaded: " + selected_file, TextOrigin::tools);
        }

        void DrawMenu(const std::vector<std::string> &agents, int index)
        {
            std::cout << "\x1b[2J\x1b[H";
            console::write_line("====================================================", TextOrigin::tools);
            console::write_line("============ Choose agent rules to load ==============", TextOrigin::tools);
            console::write_line("====================================================", TextOrigin::tools);
            console::write_line(formatter.Format("Found agent rules: %?", agents.size()), TextOrigin::tools);
            console::write_line("Use Up/Down to choose, Enter to select, Escape to quit.\n");

            for (int i = 0; i < agents.size(); ++i)
            {
                if (i == index)
                    std::cout << "\x1b[7m";

                std::cout << (i + 1) << " - " << agents[i] << "\x1b[0m\n";
            }
        }
};

#endif // COMMAND_CHANGE_RULES_INCLUDED_H