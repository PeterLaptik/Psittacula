#ifndef COMMAND_TOOLS_INCLUDED_H
#define COMMAND_TOOLS_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"
#include "format_util.h"
#include <iostream>

// Shows list of available tools
class CommandTools : public ChatCommand
{
    public:
        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {
            ActivateAlternateScreen();

            std::vector<std::pair<std::string, std::string>> tools;
            client.get()->GetToolsInfo(tools);
            console::write_line("---------------------------------------------", TextOrigin::filesystem);
            console::write_line("Available tools: ", TextOrigin::filesystem);
            for (const auto &tool : tools)
            {
                console::write_line(formatter.Format("%? - %?", tool.first, tool.second), TextOrigin::filesystem);
            }

            console::write_line("\n\nPress Enter to continue...");
            std::cin.get();

            RestoreMainScreen();
        }

        const std::string &Description() override
        {
            return m_description;
        }

    private:
        Formatter formatter;
        std::string m_description = "List of available tools.";
};

#endif // COMMAND_TOOLS_INCLUDED_H
