#ifndef COMMAND_COMPRESS_INCLUDED_H
#define COMMAND_COMPRESS_INCLUDED_H

#include "chat_command.h"

/// Compresses the context, reducing its size while preserving key information
class CommandCompress : public ChatCommand
{
    public:
        void Execute(std::unique_ptr<AiClient> &client,
            const std::vector<std::string> &args) override
        {
            client->CompressContext();
            console::write_line("Context compressed.", console::TextOrigin::filesystem);
        }

        std::string Description() override
        {
            return "Compress context to reduce size.";
        }
};

#endif // COMMAND_COMPRESS_INCLUDED_H
