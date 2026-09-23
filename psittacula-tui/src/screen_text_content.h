#ifndef SCREEN_TEXT_CONTENT_INCLUDED_H
#define SCREEN_TEXT_CONTENT_INCLUDED_H

#include "console_writer.h"
#include <string>
#include<vector>

using console::TextOrigin;

namespace tui {
    /// Screen visible text container and renderer
    class ScreenTextContent
    {
        public:
            /// Adds raw text
            void AddText(const std::string &txt, TextOrigin origin);

            void SetMaxLineLength(int length);

            int GetMaxLineLength() const;

            /// Returns text window content for a defined height
            void GetTextWindowForHeight(int height, std::vector<std::string> &acc) const;

            void Clear();

        private:
            void SplitBuffer();
            void CutByLinesBuffer();
            void Split(const std::string &s, std::vector<std::string> &vec);

            void FitInputContentToSize();
            const char* GetTextColour(TextOrigin origin);
            void PostProcess();

            int utf8_char_len(unsigned char lead);
            int Utf8Count(const std::string &text);

            struct TextLine {
                std::string txt;
                TextOrigin origin = TextOrigin::normal;
            };

            int m_text_line_max_length = 100;

            TextLine m_buffer;
            TextLine m_buffer_no_render;

            std::vector<TextLine> m_text;
            std::vector<std::string> m_buffer_rendered;
            std::vector<std::string> m_text_rendered;
    };
}

#endif // SCREEN_TEXT_CONTENT_INCLUDED_H
