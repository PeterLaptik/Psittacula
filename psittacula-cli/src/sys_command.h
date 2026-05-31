#ifndef SYS_COMMAND_INCLUDED_H
#define SYS_COMMAND_INCLUDED_H

#include <memory>

class AiClient;

class SysCommand
{
    public:
        virtual void Execute(std::unique_ptr<AiClient> &client, const std::string &args) = 0;
};

#endif // SYS_COMMAND_INCLUDED_H