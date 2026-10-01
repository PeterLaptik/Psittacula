#ifndef COMMAND_COMPRESS_INCLUDED_H
#define COMMAND_COMPRESS_INCLUDED_H

#include "chat_command.h"

/// Compresses the context, reducing its size while preserving key information
class CommandCompress : public ChatCommand
{
    public:
        using ChatCommand::ChatCommand;

        void Execute(std::unique_ptr<AiClient> &client,
            const std::vector<std::string> &args) override
        {
            if (!client.get())
            {
                console::write_line("Error: AI client is not initialized.", console::TextOrigin::error);
                return;
            }

            bool reasoning_show_val = m_app->GetShowReasoning();
            m_app->SetShowReasoning(false);

            client->CompressContext();

            console::write_line(" \n Context compressed. \n ", console::TextOrigin::tools);

            m_app->SetShowReasoning(reasoning_show_val);
        }

        std::string Description() override
        {
            return "Compress context to reduce size.";
        }
};

#endif // COMMAND_COMPRESS_INCLUDED_H
