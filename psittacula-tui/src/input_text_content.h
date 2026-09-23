#ifndef INPUT_TEXT_CONTENT_INCLUDED_H
#define INPUT_TEXT_CONTENT_INCLUDED_H

#include <string>
#include <vector>

namespace tui
{
/// Text content data holder
/// Keeps input / response text
/// Renders text output to fit to screen regions size
    class InputTextContent
    {
        public:
            InputTextContent() = default;
            ~InputTextContent() = default;

            void AddInputLine(const std::string &line);

            std::string PopBackInputLine();

            void Clear();

            void SetLineLength(int line_length);

            int GetLineLength() const;

            bool Empty() const;

            const std::vector<std::string> &GetRenderedInputLines() const;

        private:
            void FitInputContentToSize();

            int m_text_line_max_length = 10;                 // maximum line text length for the current screen state
            std::vector<std::string> m_input_lines;          // lines user input
            std::vector<std::string> m_input_lines_rendered; // lines user input rendered to fit to screen size
    };
}

#endif //! INPUT_TEXT_CONTENT_INCLUDED_H
