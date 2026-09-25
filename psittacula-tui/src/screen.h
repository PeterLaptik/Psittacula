#ifndef SCREEN_INCLUDED_H
#define SCREEN_INCLUDED_H

#include "input_text_content.h"
#include "panel.h"
#include "panel_text.h"
#include "panel_input.h"
#include "panel_status.h"
#include "console_writer.h"
#include <string>
#ifndef _WIN32
#include <termios.h>
#endif

namespace tui {
    /// Main application screen.
    /// Contains: text area and input area
    class Screen: public Panel
    {
        public:
            Screen();
            ~Screen() override = default;

            virtual void Show();

            /// Puts char from keyboard to input area
            Response PutChar(int key);

            /// Adds a text chunk to a text area
            void PutText(const std::string &txt, TextOrigin origin);

            void SetStatusLine(const std::string &status);

            void MoveSpinner();

            void Clear();

            void SetShowReasoning(bool reasoning);

            bool GetReasoning() const;

            // Commands list for autocomplete
            void UpdateCommandsAutocompleteList(std::vector<std::string> &commands);

            /// Handles resize event
            bool HandleResize();

            void GetSize(int &x, int &y) const;

            /// Callback to handle children panel changes
            void OnUpdatedChild(Panel *updated_panel = nullptr) override;

        private:
            void SetUpScreen();
#ifndef _WIN32
            struct termios g_original_termios;
            void EnableRawMode();
            void DisableRawMode();
#endif
            bool CheckScreen();
            void UpdateSize();
            void ClearScreen(int columns, int rows);
            void DrawFrame();

            struct ScreenProps
            {
                int columns = 0;
                int rows = 0;
            } m_props;

            PanelInput m_panel_input;
            PanelText m_panel_text;
            PanelStatus m_panel_status;
    };
}

#endif //! SCREEN_INCLUDED_H