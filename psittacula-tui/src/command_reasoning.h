#ifndef COMMAND_REASONING_INCLUDED_H
#define COMMAND_REASONING_INCLUDED_H

#include "chat_command.h"
#include "format_util.h"
#include "console_writer.h"

// Turns on / turns off reasoning output
class CommandReasoning : public ChatCommand
{
    public:
        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {

            if (args.empty())
            {
                console::write_line("Usage: reasoning <true|false>", TextOrigin::reasoning);
                return;
            }

            const std::string &value = args[0];

            if (value == "true")
            {
                console::write_line("Reasoning mode enabled.", TextOrigin::reasoning);
                client->SetReasoning(true);
            }
            else if (value == "false")
            {
                console::write_line("Reasoning mode disabled.", TextOrigin::reasoning);
                client->SetReasoning(false);
            }
            else
            {
                console::write_line("Invalid value. Use: true or false", TextOrigin::reasoning);
            }
        }

        const std::string &Description() override
        {
            return m_description;
        }

    private:
        Formatter formatter;
        std::string m_description = "Enable deeper reasoning output. [ARGS] - true / false";
};

#endif // !COMMAND_REASONING_INCLUDED_H