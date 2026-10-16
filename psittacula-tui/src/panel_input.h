#ifndef PANEL_INPUT_INCLUDED_H
#define PANEL_INPUT_INCLUDED_H

#include "panel.h"
#include "input_text_content.h"
#include "response.h"

namespace tui {
    /// Chat input text area
    class PanelInput: public Panel
    {
        public:
            PanelInput(Panel *parent = nullptr);

            void Draw();

            void Refresh();

            // Append char from keyboard
            Response PutChar(int key);

            // Commands list for autocomplete
            void UpdateCommandsAutocompleteList(std::vector<std::string> &commands);

            void UpdateSize(int width, int height);

            int GetInputRowsNumber() const;

            bool IsAutocompleteActive() const;

        private:
            void DrawCurrentInput();
            void DrawTextInputLine(int x, int y, const std::string &line);

            // Special keys processing
            void OnEnter();
            void OnDelete();
            void OnBackspace();
            void OnKeyRight();
            void OnKeyLeft();
            void OnKeyUp();
            void OnKeyDown();
            bool OnEscape();

            // Autocomplete drop-up list
            void UpdateAutocomplete();
            void DrawAutocompleteList();
            void AcceptAutocomplete();
            void ResetAutocomplete(bool notify);
            void FilterCommands(const std::string &prefix, std::vector<std::string> &out) const;
            void FilterFiles(const std::string &prefix, std::vector<std::string> &out) const;
            bool ExtractFileQuery(const std::string &text, std::string &query) const;

            // UTF-8 helpers
            size_t Utf8ByteOffset(const std::string &text, int codepoint_index);
            std::string Utf8Encode(int codepoint);
            int utf8_char_len(unsigned char lead);
            int Utf8Count(const std::string &text);

            std::string JoinInputString(const std::vector<std::string> &v, const std::string &delim);

            InputTextContent m_text_content;
            std::string m_input_line;

            int m_input_cursor_pos_x = 1;
            int m_input_cursor_pos_y = 0;
            int m_input_line_text_length = 0;

            // Autocomplete state
            std::vector<std::string> m_autocomplete_items;
            int m_autocomplete_selected = 0;
            bool m_autocomplete_active = false;
            bool m_autocomplete_file_mode = false;

            std::vector<std::string> m_commands_list;

            constexpr static int kTextPaddingLeft = 3;
            constexpr static int kTextPaddingRight = 1;
            constexpr static int kAutocompleteMaxVisible = 6;
    };
}

#endif // PANEL_INPUT_INCLUDED_H