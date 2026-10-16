#ifndef CONFIRM_DIALOGUE_INCLUDED_H
#define CONFIRM_DIALOGUE_INCLUDED_H

#include "screen.h"
#include <atomic>
#include <string>
#include <mutex>
#include <condition_variable>

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

            void Cancel();

        private:
            Screen &m_screen;
            std::atomic<bool> m_is_shown = false;
            int m_pressed_key = 0;

            std::mutex m_key_guard;
            std::condition_variable m_cv;
    };
}

#endif // CONFIRM_DIALOGUE_INCLUDED_H