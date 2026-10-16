#ifndef COMMAND_REDO_INCLUDED_H
#define COMMAND_REDO_INCLUDED_H

#include "chat_command.h"

/// Redo last tool
class CommandRedo : public ChatCommand
{
    public:
        using ChatCommand::ChatCommand;

        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {
            if(!client.get())
            {
                console::write_line("Error: AiClient is not initialized / no connection to LLM", TextOrigin::error);
                return;
            }

            client->ToolRedo();
        }

        std::string Description() override
        {
            return "Redo last tool call.";
        }
};

#endif // COMMAND_REDO_INCLUDED_H
