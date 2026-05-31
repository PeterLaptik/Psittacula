#ifndef CHAT_COMMAND_INCLUDED_H
#define CHAT_COMMAND_INCLUDED_H

#include "ai_client.h"
#include "console_writer.h"
#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include <iterator>

using console::TextOrigin;

/// Common interface for a chat command
/// To call chat command use '/' prefix in a console
class ChatCommand
{
    public:
        ChatCommand() = default;

        virtual ~ChatCommand() = default;

        virtual void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) = 0;

        virtual const std::string& Description() = 0;

    protected:
        // Sets alternate screen buffer for command interractions, if necessary
        void  ActivateAlternateScreen()
        {
            std::cout << "\x1b[?1049h\x1b[2J\x1b[H";
        }
        // Sets back to main dialogue screen
        void RestoreMainScreen()
        {
            std::cout << "\x1b[?1049l";
        }

};

class ChatCommandDispatcher
{
    public:
        ChatCommandDispatcher() = default;

        ~ChatCommandDispatcher() = default;

        void RegisterCommand(const std::string &command_name, ChatCommand *command)
        {
            // Force lowercase for commands names
            std::string cmd;
            std::transform(command_name.begin(), command_name.end(), std::back_inserter(cmd),
                [](unsigned char c) { return std::tolower(c); });

            m_commands[command_name] = std::unique_ptr<ChatCommand>(command);
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

            auto it = m_commands.find(command_name);
            if (it != m_commands.end())
            {
                it->second->Execute(client, args);
            }
            else
            {
                console::write_line("Unknown command: " + command_name, TextOrigin::error);
            }
        }

        // Shows list of aviable commands
        void ShowHelp()
        {
            std::cout << "\x1b[?1049h\x1b[2J\x1b[H";
            std::vector<std::pair<std::string, std::string>> cmd_list;
            for (auto &cmd : m_commands)
            {
                cmd_list.push_back(std::make_pair(cmd.first, cmd.second->Description()));
            }

            std::sort(cmd_list.begin(), cmd_list.end(), [](auto item_1, auto item_2) {
                    return item_1.first < item_2.first;
                });

            console::write_line("---------------------------------------------", TextOrigin::filesystem);
            console::write_line("Avialable commands:", TextOrigin::filesystem);
            for (auto &cmd_desc : cmd_list)
            {
                console::write_line("\033[1m" + cmd_desc.first + " -\033[0m " + cmd_desc.second);
            }

            console::write_line("\n\nPress Enter to continue...");
            std::cin.get();

            std::cout << "\x1b[?1049l";
        }

    private:
        std::map<std::string, std::unique_ptr<ChatCommand>> m_commands;
};

#endif // !CHAT_COMMAND_INCLUDED_H

