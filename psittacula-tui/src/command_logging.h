#ifndef COMMAND_LOGGING_INCLUDED_H
#define COMMAND_LOGGING_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"
#include <algorithm>
#include <ctype.h>

/// Turns on / off the dialogue logging in AiClient (autosave of the full
/// JSON dialogue body into the workdir /logs directory)
class CommandLogging : public ChatCommand
{
    public:
        using ChatCommand::ChatCommand;

        void Execute(std::unique_ptr<AiClient> &client,
            const std::vector<std::string> &args) override
        {
            if (!client.get())
            {
                console::write_line("Error: AiClient is not initialized / no connection to LLM", TextOrigin::error);
                return;
            }

            if (args.size() != 1)
            {
                console::write_line("Usage: log <true|false>", TextOrigin::tools);
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
                client->SetLog(true);
                console::write_line("Dialogue logging is enabled.", TextOrigin::tools);
            }
            else if (value == "false" || value == "0" || value == "off")
            {
                client->SetLog(false);
                console::write_line("Dialogue logging is disabled.", TextOrigin::tools);
            }
            else
            {
                console::write_line("Invalid value. Use: true or false", TextOrigin::error);
            }
        }

        std::string Description() override
        {
            return "Turns dialogue logging (autosave to the logs dir) on / off.\n\t[ARG] - true / false";
        }
};

#endif // COMMAND_LOGGING_INCLUDED_H
