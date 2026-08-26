#ifndef CHAT_COMMAND_DISPATCHER_INCLUDED_H
#define CHAT_COMMAND_DISPATCHER_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"
#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <memory>
#include <cstddef>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

class AiClient;

// Raw terminal input: puts terminal in raw mode, reads one byte, restores mode.
// Must be declared before any class that uses it.
static int GetKey()
{
#ifdef _WIN32
    return _getch();
#else
    struct termios old_tios, new_tios;
    tcgetattr(STDIN_FILENO, &old_tios);
    new_tios = old_tios;
    new_tios.c_lflag &= ~(ICANON | ECHO);
    new_tios.c_cc[VMIN] = 1;
    new_tios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tios);

    int c = fgetc(stdin);

    tcsetattr(STDIN_FILENO, TCSANOW, &old_tios);
    return c;
#endif
}

/// Keeps and dispatches chat commands
class ChatCommandDispatcher
{
    public:
        ChatCommandDispatcher() = default;

        ~ChatCommandDispatcher() = default;

        void RegisterCommand(const std::string &command_name, std::unique_ptr<ChatCommand> command)
        {
            // Force lowercase for commands names
            std::string cmd;
            std::transform(command_name.begin(), command_name.end(), std::back_inserter(cmd),
                [](unsigned char c) { return std::tolower(c); });

            m_commands[cmd] = std::move(command);
        }

        void DispatchCommand(const std::string &command_name, const std::vector<std::string> &args, std::unique_ptr<AiClient> &client)
        {
            // Force lowercase for commands names
            std::string cmd_name;
            std::transform(command_name.begin(), command_name.end(), std::back_inserter(cmd_name),
                [](unsigned char c) { return std::tolower(c); });

            if (cmd_name == "help" || cmd_name == "h")
            {
                ShowHelp();
                return;
            }

            auto it = m_commands.find(cmd_name);
            if (it != m_commands.end())
            {
                it->second->Execute(client, args);
            }
            else
            {
                console::write_line("\rUnknown command: " + command_name, console::TextOrigin::error);
            }
        }

        // Shows list of available commands with descriptions in an interactive menu
        std::string SelectCommand()
        {
            ActivateAlternateScreen();

            std::vector<std::pair<std::string, std::string>> cmd_list;
            for (auto &cmd : m_commands)
            {
                cmd_list.emplace_back(cmd.first, cmd.second->Description());
            }

            std::sort(cmd_list.begin(), cmd_list.end(), [](auto item_1, auto item_2) {
                return item_1.first < item_2.first;
                });

            int index = 0;
            DrawCommandMenu(cmd_list, index);

            while (true)
            {
#ifdef _WIN32
                int c = _getch();
                if (c == 224)
                {
                    int arrow = _getch();
                    if (arrow == 72 && index > 0) index--;
                    else if (arrow == 80 && index < cmd_list.size() - 1) index++;
                    DrawCommandMenu(cmd_list, index);
                }
                else if (c == 27)
                {
                    // Escape to quit without selecting
                    RestoreMainScreen();
                    return "";
                }
                else if (c == 13)
                {
                    // Enter to select command
                    break;
                }
#else
                int c = GetKey();
                if (c == '\x1b')
                {
                    int c1 = GetKey();
                    if (c1 == '[')
                    {
                        int c2 = GetKey();
                        if (c2 == 'A' && index > 0) index--;
                        else if (c2 == 'B' && index < cmd_list.size() - 1) index++;
                        DrawCommandMenu(cmd_list, index);
                    }
                }
                else if (c == '\x1b' || c == 27)
                {
                    // Escape to quit without selecting
                    RestoreMainScreen();
                    return "";
                }
                else if (c == '\n' || c == '\r')
                {
                    // Enter to select command
                    break;
                }
#endif
            }

            // Selected command
            std::string selected_cmd = cmd_list[index].first;
            std::string selected_desc = cmd_list[index].second;

            RestoreMainScreen();
            return selected_cmd;
        }

        void DrawCommandMenu(const std::vector<std::pair<std::string, std::string>> &commands, int index)
        {
            std::cout << "\x1b[2J\x1b[H";
            console::write_line("--------------------------------------------------------------", console::TextOrigin::filesystem);
            console::write_line("\033[1mSelect a command to execute\033[0m");
            console::write_line("Use Up/Down to navigate, Enter to select, Escape to quit.\n");

            for (int i = 0; i < commands.size(); ++i)
            {
                if (i == index)
                    std::cout << "\x1b[7m";

                std::cout << "\033[1m" << commands[i].first << "\033[0m" << " - " << commands[i].second;
                if (i == index)
                    std::cout << "\x1b[0m";
                else
                    std::cout << "\033[0m";
                std::cout << "\n";
            }
        }

        // Shows list of aviable commands (non-interactive)
        void ShowHelp()
        {
            std::cout << "\x1b[?1049h\x1b[2J\x1b[H";
            std::vector<std::pair<std::string, std::string>> cmd_list;
            for (auto &cmd : m_commands)
            {
                cmd_list.emplace_back(cmd.first, cmd.second->Description());
            }

            std::sort(cmd_list.begin(), cmd_list.end(), [](auto item_1, auto item_2) {
                return item_1.first < item_2.first;
                });

            console::write_line("---------------------------------------------", console::TextOrigin::filesystem);
            console::write_line("\033[1mAvialable commands:\033[0m");
            for (auto &cmd_desc : cmd_list)
            {
                console::write_line("\033[1m" + cmd_desc.first + " -\033[0m " + cmd_desc.second);
            }

            console::write_line("\033[1mq -\033[0m Clear active query.");
            console::write_line("\033[1mexit -\033[0m Exit form the program.");

            console::write_line("\n\nPress Enter to continue...");
            std::cin.get();

            std::cout << "\x1b[?1049l";
        }

    private:
        // Sets alternate screen buffer for command interactive mode, if necessary
        void  ActivateAlternateScreen() const
        {
            std::cout << "\x1b[?1049h\x1b[2J\x1b[H";
        }
        // Sets back to main dialogue screen
        void RestoreMainScreen() const
        {
            std::cout << "\x1b[?1049l";
        }

        std::map<std::string, std::unique_ptr<ChatCommand>> m_commands;
};

#endif // CHAT_COMMAND_DISPATCHER_INCLUDED_H