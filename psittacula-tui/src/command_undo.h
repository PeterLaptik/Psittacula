#ifndef COMMAND_UNDO_INCLUDED_H
#define COMMAND_UNDO_INCLUDED_H

#include "chat_command.h"
#include "format_util.h"
#include "console_writer.h"

// Undo last tool
class CommandUndo : public ChatCommand
{
    public:
        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {
            client->ToolUndo();
        }

        const std::string &Description() override
        {
            return m_description;
        }

    private:
        std::string m_description = "Undo last tool call.";
};

#endif // !COMMAND_UNDO_INCLUDED_H
