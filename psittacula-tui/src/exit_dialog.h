#ifndef EXIT_DIALOG_INCLUDED_H
#define EXIT_DIALOG_INCLUDED_H

#include "screen.h"
#include "keyboard.h"

namespace tui {
    /// Handles exit menu: asks the user to confirm quitting the application
    class ExitDialog
    {
        public:
            ExitDialog(Screen &screen, Keyboard &keyboard);

            /// Shows the exit menu in an alternate screen buffer,
            /// returns true when the user confirmed to quit
            bool Confirm();

        private:
            Screen &m_screen;
            Keyboard &m_keyboard;
    };
}

#endif // EXIT_DIALOG_INCLUDED_H
