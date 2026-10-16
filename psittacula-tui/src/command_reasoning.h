#ifndef COMMAND_REASONING_INCLUDED_H
#define COMMAND_REASONING_INCLUDED_H

#include "chat_command.h"
#include "format_util.h"
#include "console_writer.h"
#include <ctype.h>

/// Turns on / off resoning text in an output
class CommandReasoning : public ChatCommand
{
    public:
        using ChatCommand::ChatCommand;

        void Execute(std::unique_ptr<AiClient> &client,
            const std::vector<std::string> &args) override
        {
            if (args.size() != 1)
            {
                console::write_line("Usage: reasoning <true|false>", TextOrigin::tools);
                return;
            }

            auto to_lower = [](std::string s) {
                std::transform(s.begin(), s.end(), s.begin(),
                    [](unsigned char c) { return std::tolower(c); });
                return s;
                };

            std::string value = to_lower(args[0]);

            if (value == "true" || value == "1" || value == "on")
            {
                m_app->SetShowReasoning(true);
                console::write_line("Reasoning mode is enabled.", TextOrigin::tools);
            }
            else if (value == "false" || value == "0" || value == "off")
            {
                m_app->SetShowReasoning(false);
                console::write_line("Reasoning mode is disabled.", TextOrigin::tools);
            }
            else
            {
                console::write_line("Invalid value. Use: true or false", TextOrigin::error);
            }
        }

        std::string Description() override
        {
            return "Enable reasoning text output.\n\t[ARG] - true / false";
        }
};

#endif // COMMAND_REASONING_INCLUDED_H
