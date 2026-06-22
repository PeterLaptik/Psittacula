#ifndef COMMAND_CLEAN_CONTEXT_INCLUDED_H
#define COMMAND_CLEAN_CONTEXT_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"

/// Clears the context of the current session
class CommandCleanContext : public ChatCommand
{
    public:
        void Execute(std::unique_ptr<AiClient> &client,
            const std::vector<std::string> &args) override
        {
            client->ClearContext();
            console::write_line("Context cleared.", TextOrigin::filesystem);
        }

        std::string Description() override
        {
            return "Clears the current conversation context.";
        }
};

#endif // COMMAND_CLEAN_CONTEXT_INCLUDED_H