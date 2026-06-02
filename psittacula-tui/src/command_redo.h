#ifndef COMMAND_REDO_INCLUDED_H
#define COMMAND_REDO_INCLUDED_H

#include "chat_command.h"
#include "format_util.h"
#include "console_writer.h"

// Redo last tool
class CommandRedo : public ChatCommand
{
    public:
        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {
            client->ToolRedo();
        }

        std::string Description() override
        {
            return "Redo last tool call.";
        }
};

#endif // !COMMAND_REDO_INCLUDED_H
