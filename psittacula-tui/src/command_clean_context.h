#ifndef COMMAND_CLEAN_CONTEXT_INCLUDED_H
#define COMMAND_CLEAN_CONTEXT_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"

/// Clears the context of the current session
class CommandCleanContext : public ChatCommand
{
    public:
        using ChatCommand::ChatCommand;

        void Execute(std::unique_ptr<AiClient> &client,
            const std::vector<std::string> &args) override
        {
            client->ClearContext();
            console::write_line("Context cleared.", TextOrigin::filesystem);
            console::write_line("-----", TextOrigin::normal);
            console::write(GetLogo());
        }

        std::string Description() override
        {
            return "Clears the current conversation context.";
        }

    private:
        #ifndef PSITTACULA_APP_VERSION
        #define PSITTACULA_APP_VERSION "unknown"
        #endif

        std::string GetLogo()
        {
            std::string logo =
                R"(_______________________________________________________
      / __\ ___\//_ _/_ _// | /__\//  ///   / |      
     / /_// /  // //  // //||//  //  ///   //||     
    / ___/__ \// //  // //_||/  //  ///   //_||    
   / /  ____\// //  // /___ |\_//__///__ /___ |   
 __\/__/____//_//__//_//___||_/\___/____\/___||__
    )";

            logo += "Version: ";
            logo += PSITTACULA_APP_VERSION;
            logo += "\n    Written by Peter Laptik";
            logo += "\n\033[36m    Input /help for information about commands\033[0m";
            logo += "\n\033[36m    or use / for interractive command choise\033[0m \n \n";
            return logo;
        }
};

#endif // COMMAND_CLEAN_CONTEXT_INCLUDED_H