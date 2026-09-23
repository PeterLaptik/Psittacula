#ifndef PANEL_TEXT_INCLUDED_H
#define PANEL_TEXT_INCLUDED_H

#include "panel.h"
#include "screen_text_content.h"
#include "console_writer.h"

namespace tui {
    /// Chat text content area
    class PanelText: public Panel
    {
        public:
            PanelText(Panel *parent = nullptr);

            /// Add text (chunk): LLM response, tool response info, etc.
            void AddText(const std::string &txt, TextOrigin origin);

            void Draw();

            void Clear();

            void Refresh();

            void UpdateSize(int width, int height);

            /// Scrolls visible text: up = towards older lines, down = back to the newest
            void ScrollUp(int lines);
            void ScrollDown(int lines);
            void ScrollPageUp();
            void ScrollPageDown();

        private:
            int TextWindowHeight() const;
            int MaxViewShift() const;

            ScreenTextContent m_text_content;
            int m_view_shift = 0;

            constexpr static int kTextPaddingLeft = 2;
            constexpr static int kTextPaddingRight = 2;
    };
}
#endif // PANEL_TEXT_INCLUDED_H