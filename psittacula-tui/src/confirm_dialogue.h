#ifndef CONFIRM_DIALOGUE_INCLUDED_H
#define CONFIRM_DIALOGUE_INCLUDED_H

#include "screen.h"
#include <string>

class Screen;

namespace tui {
    /// Handles confirmation menu
    class ConfirmDialogue
    {
        public:
            explicit ConfirmDialogue(Screen &screen);

            /// Shows the confirmation menu (y/n key press waiting)
            bool Confirm(const std::string &message);

            void PutCharFromKeyboard(int key);

            bool IsShown() const;

        private:
            Screen &m_screen;
            bool m_is_shown = false;
            int m_pressed_key = 0;
    };
}

#endif // CONFIRM_DIALOGUE_INCLUDED_H