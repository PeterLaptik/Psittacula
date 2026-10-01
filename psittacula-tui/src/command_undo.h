#ifndef COMMAND_UNDO_INCLUDED_H
#define COMMAND_UNDO_INCLUDED_H

#include "chat_command.h"

// Undo last tool
class CommandUndo : public ChatCommand
{
    public:
        using ChatCommand::ChatCommand;

        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {
            if (!client.get())
            {
                console::write_line("Error: AiClient is not initialized / no connection to LLM", TextOrigin::error);
                return;
            }

            client->ToolUndo();
        }

        std::string Description() override
        {
            return "Undo last tool call.";
        }
};

#endif // COMMAND_UNDO_INCLUDED_H
