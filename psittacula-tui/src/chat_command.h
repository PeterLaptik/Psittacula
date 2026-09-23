#ifndef CHAT_COMMAND_INCLUDED_H
#define CHAT_COMMAND_INCLUDED_H

#include "ai_client.h"
#include "app.h"
#include "console_writer.h"
#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include <iterator>

using console::TextOrigin;

/// Common interface for a chat command
/// To call chat command use '/' prefix in a console
class ChatCommand
{
    public:
        ChatCommand(tui::App *app)
            : m_app(app)
        { }

        virtual ~ChatCommand() = default;

        virtual void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) = 0;

        virtual std::string Description() = 0;

    protected:
        tui::App *m_app;

        // Sets alternate screen buffer for command interactive mode, if necessary
        void  ActivateAlternateScreen() const
        {
            console::set_up_console();
            std::cout << "\x1b[?1049h\x1b[2J\x1b[H";
        }
        // Sets back to main dialogue screen
        void RestoreMainScreen() const
        {
            std::cout << "\x1b[?1049l";
            console::set_up_console(m_app);
        }

};

#endif // CHAT_COMMAND_INCLUDED_H

