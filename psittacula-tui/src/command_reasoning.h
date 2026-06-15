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
        void Execute(std::unique_ptr<AiClient> &client,
            const std::vector<std::string> &args) override
        {
            if (args.size() != 1)
            {
                console::write_line("Usage: reasoning <true|false>", TextOrigin::reasoning);
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
                client->SetReasoning(true);
                console::write_line("Reasoning mode enabled.", TextOrigin::reasoning);
            }
            else if (value == "false" || value == "0" || value == "off")
            {
                client->SetReasoning(false);
                console::write_line("Reasoning mode disabled.", TextOrigin::reasoning);
            }
            else
            {
                console::write_line("Invalid value. Use: true or false", TextOrigin::error);
            }
        }

        std::string Description() override
        {
            return "Enable deeper reasoning output. [ARGS] - true / false";
        }
};

#endif // COMMAND_REASONING_INCLUDED_H
