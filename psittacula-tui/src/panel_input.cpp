#include "panel_input.h"
#include "keyboard.h"
#include "working_dir.h"
#include <algorithm>
#include <iostream>
#include <sstream>

tui::PanelInput::PanelInput(Panel *parent)
    : Panel(parent)
{ }

void tui::PanelInput::Draw()
{
    // Draw frame
    MoveCursorTo(m_anchor_x, m_anchor_y);
    for (int row = 0; row <= m_height; ++row)
    {
        bool top = false; // Or use row == 0 if no border for upper frame
        bool bottom = row == m_height;

        for (int col = 0; col < m_width; ++col)
        {
            bool left = col == 0;
            bool right = col == m_width - 1;

            if ((top || bottom) && (left || right))
                std::cout << "\033[34m" << '+' << "\033[0m";
            else if (top || bottom)
                std::cout << "\033[34m" << '-' << "\033[0m";
            else if (left || right)
                std::cout << "\033[34m" << '|' << "\033[0m";
            else
                std::cout << ' ';
        }
    }

    // Draw input text
    DrawCurrentInput();

    // Draw autocomplete drop-up list over the text panel, just above the input frame
    DrawAutocompleteList();
}

void tui::PanelInput::Refresh()
{
}

tui::Response tui::PanelInput::PutChar(int key)
{
    Response rsp_ok;

    if (key == Keyboard::Keys::eof)
        return rsp_ok;

    // Cursor validating
    m_input_line_text_length = Utf8Count(m_input_line);
    if (m_input_cursor_pos_x <= 1)
        m_input_cursor_pos_x = 1;
    if (m_input_cursor_pos_x - 1 > m_input_line_text_length)
        m_input_cursor_pos_x = m_input_line_text_length + 1;

    if (key == Keyboard::Keys::keyUp)
    {
        OnKeyUp();
        return rsp_ok;
    }

    if (key == Keyboard::Keys::keyDown)
    {
        OnKeyDown();
        return rsp_ok;
    }

    if (key == Keyboard::Keys::keyEnter)
    {
        // Enter accepts the highlighted command while the list is shown
        if (m_autocomplete_active)
        {
            AcceptAutocomplete();
            return rsp_ok;
        }

        // Should the text be sent as a message (double enter click)?
        auto &input_lines = m_text_content.GetRenderedInputLines();
        if (!input_lines.empty())
        {
            bool are_last_lines_empty = m_input_line.empty();
            if (are_last_lines_empty)
            {
                std::string full_message_text = JoinInputString(input_lines, "\n");
                m_text_content.Clear();
                NotifyParentAboutChanges();
                return Response(ResponseResult::enter , full_message_text);
            }
        }

        // Is command input
        if (input_lines.empty() && m_input_line.size() > 0 && m_input_line[0] == '/')
        {
            Response cmd_rsp{ ResponseResult::command, m_input_line };
            m_input_line.clear();
            m_input_cursor_pos_x = 1;
            NotifyParentAboutChanges();
            return cmd_rsp;
        }
        
        OnEnter();

        NotifyParentAboutChanges();
        return rsp_ok;
    }

    if (key == Keyboard::Keys::keyBackSpace)
    {
        OnBackspace();
        return rsp_ok;
    }

    if (key == Keyboard::Keys::keyLeft)
    {
        OnKeyLeft();
        return rsp_ok;
    }

    if (key == Keyboard::Keys::keyRight)
    {
        OnKeyRight();
        return rsp_ok;
    }

    if (key == Keyboard::Keys::keyDel)
    {
        OnDelete();
        return rsp_ok;
    }

    // On escape (EtX/Eot/ESC)
    if (key == 3 || key == 4 || key == 27)
    {
        bool do_escape = OnEscape();
        ResponseResult res = do_escape ? ResponseResult::escape : ResponseResult::ok;
        return Response(res);
    }

    if (key < 0x20 || key == 0x7F)
        return rsp_ok;

    if ((key >= 0xD800 && key <= 0xDFFF) || key > 0x10FFFF)
        return rsp_ok;

    int insert_position = m_input_cursor_pos_x - 1;
    if (insert_position < 0)
        insert_position = 0;
    if (insert_position > m_input_line_text_length)
        insert_position = m_input_line_text_length;
    size_t insert_byte = Utf8ByteOffset(m_input_line, insert_position);
    m_input_line.insert(insert_byte, Utf8Encode(key));
    ++m_input_cursor_pos_x;
    DrawTextInputLine(m_input_cursor_pos_x, m_input_cursor_pos_y, m_input_line);
    UpdateAutocomplete();
    return rsp_ok;
}

void tui::PanelInput::UpdateCommandsAutocompleteList(std::vector<std::string> &commands)
{
    m_commands_list.clear();
    m_commands_list.insert(m_commands_list.begin(), commands.begin(), commands.end());
}

void tui::PanelInput::UpdateSize(int width, int height)
{
    m_width = width;
    m_height = height;

    // Render on resize
    int working_line_length = m_text_content.GetLineLength();
    int actual_line_length = m_width - kTextPaddingLeft - kTextPaddingRight;
    if (working_line_length != actual_line_length)
    {
        m_text_content.SetLineLength(actual_line_length);
    }
}

int tui::PanelInput::GetInputRowsNumber() const
{
    return static_cast<int>(m_text_content.GetRenderedInputLines().size());
}

bool tui::PanelInput::IsAutocompleteActive() const
{
    return m_autocomplete_active;
}

void tui::PanelInput::DrawCurrentInput()
{
    m_input_cursor_pos_y = m_anchor_y + m_height - 1;
    const auto &rendered_lines = m_text_content.GetRenderedInputLines();
    int line_qnt = static_cast<int>(rendered_lines.size());
    for (int line_pos = 0; line_pos < line_qnt; ++line_pos)
    {
        DrawTextInputLine(1, m_anchor_y + line_pos, rendered_lines[line_pos]);
    }

    DrawTextInputLine(m_input_cursor_pos_x, m_input_cursor_pos_y, m_input_line);
}

void tui::PanelInput::DrawTextInputLine(int x, int y, const std::string &line)
{
    if (y < 0)
        return;

    if (x < 1)
        x = 1;

    int clear_count = m_width - kTextPaddingRight - kTextPaddingLeft;
    if (clear_count < 0)
        clear_count = 0;

    MoveCursorTo(kTextPaddingLeft, y);
    std::cout << std::string(static_cast<size_t>(clear_count), ' ') << '\r' << std::flush;

    MoveCursorTo(kTextPaddingLeft, y);
    std::cout << '>' << line;
    MoveCursorTo(x + kTextPaddingLeft, y);
    std::cout.flush();
}

void tui::PanelInput::OnEnter()
{
    ResetAutocomplete(false);

    if (m_text_content.GetRenderedInputLines().empty() && m_input_line.empty())
        return;

    m_input_cursor_pos_x = 1;
    m_text_content.SetLineLength(m_width - kTextPaddingLeft - kTextPaddingRight);
    m_text_content.AddInputLine(m_input_line);
    m_input_line.clear();
    DrawCurrentInput();
}

void tui::PanelInput::OnDelete()
{
    // Delete char under cursor: cursor 1 = before char 0, so delete index pos-1.
    int position = m_input_cursor_pos_x - 1;
    if (position >= 0 && position < m_input_line_text_length && !m_input_line.empty())
    {
        size_t byte_pos = Utf8ByteOffset(m_input_line, position);
        if (byte_pos < m_input_line.size())
            m_input_line.erase(byte_pos, utf8_char_len(static_cast<unsigned char>(m_input_line[byte_pos])));
        m_input_line_text_length = Utf8Count(m_input_line);
        if (m_input_cursor_pos_x - 1 > m_input_line_text_length)
            m_input_cursor_pos_x = m_input_line_text_length + 1;
    }

    DrawTextInputLine(m_input_cursor_pos_x, m_input_cursor_pos_y, m_input_line);
    UpdateAutocomplete();
}

void tui::PanelInput::OnBackspace()
{
    if (!m_input_line.empty())
    {
        int text_length = Utf8Count(m_input_line);
        int position = m_input_cursor_pos_x - 2;
        if (position >= 0 && position < text_length)
        {
            size_t byte_pos = Utf8ByteOffset(m_input_line, position);
            if (byte_pos < m_input_line.size())
            {
                m_input_line.erase(byte_pos, utf8_char_len(static_cast<unsigned char>(m_input_line[byte_pos])));
                --m_input_cursor_pos_x;
            }
        }

        DrawTextInputLine(m_input_cursor_pos_x, m_input_cursor_pos_y, m_input_line);
    }
    else if (m_input_line.empty() && !m_text_content.GetRenderedInputLines().empty())
    {
        m_input_line = m_text_content.PopBackInputLine();
        m_input_cursor_pos_x = Utf8Count(m_input_line) + 1;
        NotifyParentAboutChanges();
        DrawCurrentInput();
    }

    UpdateAutocomplete();
}

void tui::PanelInput::OnKeyRight()
{
    if (m_input_cursor_pos_x <= m_input_line_text_length)
        ++m_input_cursor_pos_x;

    DrawTextInputLine(m_input_cursor_pos_x, m_input_cursor_pos_y, m_input_line);
}

void tui::PanelInput::OnKeyLeft()
{
    if (m_input_cursor_pos_x > 1)
        --m_input_cursor_pos_x;

    DrawTextInputLine(m_input_cursor_pos_x, m_input_cursor_pos_y, m_input_line);
}

void tui::PanelInput::OnKeyUp()
{
    if (!m_autocomplete_active || m_autocomplete_items.empty())
        return;

    if (m_autocomplete_selected > 0)
        --m_autocomplete_selected;
    else
        m_autocomplete_selected = static_cast<int>(m_autocomplete_items.size()) - 1;

    NotifyParentAboutChanges();
}

void tui::PanelInput::OnKeyDown()
{
    if (!m_autocomplete_active || m_autocomplete_items.empty())
        return;

    m_autocomplete_selected = (m_autocomplete_selected + 1) % static_cast<int>(m_autocomplete_items.size());

    NotifyParentAboutChanges();
}

bool tui::PanelInput::OnEscape()
{
    if (m_autocomplete_active)
    {
        ResetAutocomplete(true);
        return false;
    }

    if (m_text_content.Empty() && m_input_line.empty())
    {
        return true;
    }
    else
    {
        if (!m_input_line.empty())
            m_input_line.clear();
        else
            m_text_content.PopBackInputLine();

        m_input_cursor_pos_x = 1;
        NotifyParentAboutChanges();
        return false;
    }
}

void tui::PanelInput::UpdateAutocomplete()
{
    bool on_first_line = m_text_content.GetRenderedInputLines().empty();
    bool starts_slash = on_first_line && !m_input_line.empty() && m_input_line[0] == '/';

    std::vector<std::string> filtered;
    bool file_mode = false;
    if (starts_slash)
    {
        FilterCommands(m_input_line, filtered);
    }
    else
    {
        std::string query;
        if (ExtractFileQuery(m_input_line, query))
        {
            file_mode = true;
            FilterFiles(query, filtered);
        }
    }

    bool new_active = !filtered.empty();
    bool changed = (m_autocomplete_active != new_active) ||
                   (new_active && (filtered != m_autocomplete_items || m_autocomplete_selected != 0));

    m_autocomplete_items = std::move(filtered);
    m_autocomplete_active = new_active;
    m_autocomplete_file_mode = new_active && file_mode;
    m_autocomplete_selected = 0;

    if (changed)
        NotifyParentAboutChanges();
}

void tui::PanelInput::FilterCommands(const std::string &prefix, std::vector<std::string> &out) const
{
    for (const std::string &cmd : m_commands_list)
    {
        if (cmd.compare(0, prefix.size(), prefix) == 0)
            out.push_back(cmd);
    }
}

void tui::PanelInput::FilterFiles(const std::string &prefix, std::vector<std::string> &out) const
{
    auto project_files = WorkingDir::GetInstance().GetProjectFilesList();
    for (const auto &file : project_files)
    {
        if (file.name.compare(0, prefix.size(), prefix) == 0)
            out.push_back(file.name);
    }
}

bool tui::PanelInput::ExtractFileQuery(const std::string &text, std::string &query) const
{
    // File autocomplete triggers on a file name enter (' @')
    size_t at_pos = text.rfind('@');
    if (at_pos == std::string::npos)
        return false;

    if (at_pos == 0 || text[at_pos - 1] != ' ')
        return false;

    query = text.substr(at_pos + 1);
    if (query.find(' ') != std::string::npos)
        return false;

    return true;
}

void tui::PanelInput::ResetAutocomplete(bool notify)
{
    m_autocomplete_active = false;
    m_autocomplete_selected = 0;
    m_autocomplete_items.clear();

    if (notify)
        NotifyParentAboutChanges();
}

void tui::PanelInput::AcceptAutocomplete()
{
    if (!m_autocomplete_active ||
        m_autocomplete_selected < 0 ||
        m_autocomplete_selected >= static_cast<int>(m_autocomplete_items.size()))
        return;

    std::string selected = m_autocomplete_items[static_cast<size_t>(m_autocomplete_selected)];

    if (m_autocomplete_file_mode)
    {
        auto project_files = WorkingDir::GetInstance().GetProjectFilesList();
        for (auto &file : project_files)
        {
            if (file.name == selected)
            {
                selected = file.path;
                break;
            }
        }

        // Replace only the '@' token, keep the rest of the line
        size_t at_pos = m_input_line.rfind('@');
        if (at_pos == std::string::npos)
            m_input_line = selected;
        else
            m_input_line = m_input_line.substr(0, at_pos - 1) + ' ' + selected;
    }
    else
    {
        m_input_line = selected;
    }

    m_input_line_text_length = Utf8Count(m_input_line);
    m_input_cursor_pos_x = m_input_line_text_length + 1;

    ResetAutocomplete(true);
}

void tui::PanelInput::DrawAutocompleteList()
{
    if (!m_autocomplete_active || m_autocomplete_items.empty() || m_anchor_y < 1)
        return;

    int total = static_cast<int>(m_autocomplete_items.size());
    int shown = std::min(total, kAutocompleteMaxVisible);
    shown = std::min(shown, m_anchor_y);

    int first = 0;
    if (m_autocomplete_selected >= kAutocompleteMaxVisible)
        first = m_autocomplete_selected - kAutocompleteMaxVisible + 1;
    if (first + shown > total)
        first = total - shown;
    if (first < 0)
        first = 0;

    int list_width = 0;
    for (int i = first; i < first + shown; ++i)
        list_width = std::max(list_width, static_cast<int>(m_autocomplete_items[static_cast<size_t>(i)].size()));
    list_width += 2;

    int max_width = m_width - kTextPaddingLeft - 1;
    if (max_width < 1)
        return;
    if (list_width > max_width)
        list_width = max_width;

    for (int visible = 0; visible < shown; ++visible)
    {
        int idx = first + visible;
        int y = m_anchor_y - shown + visible;

        std::string row = ' ' + m_autocomplete_items[static_cast<size_t>(idx)];
        if (static_cast<int>(row.size()) < list_width)
            row.resize(static_cast<size_t>(list_width), ' ');

        MoveCursorTo(kTextPaddingLeft, y);
        if (idx == m_autocomplete_selected)
            std::cout << "\033[7m" << row << "\033[0m";
        else
            std::cout << row;
    }

    MoveCursorTo(m_input_cursor_pos_x + kTextPaddingLeft, m_input_cursor_pos_y);
    std::cout.flush();
}

size_t tui::PanelInput::Utf8ByteOffset(const std::string &text, int codepoint_index)
{
    if (codepoint_index < 0)
        return 0;
    size_t offset = 0;
    for (int i = 0; i < codepoint_index && offset < text.size(); ++i)
    {
        size_t step = static_cast<size_t>(utf8_char_len(static_cast<unsigned char>(text[offset])));
        if (offset + step > text.size())
            return text.size();
        offset += step;
    }
    return offset;
}

std::string tui::PanelInput::Utf8Encode(int codepoint)
{
    std::string out;
    if (codepoint < 0x80)
    {
        out += static_cast<char>(codepoint);
    }
    else if (codepoint < 0x800)
    {
        out += static_cast<char>(0xC0 | (codepoint >> 6));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    else if (codepoint < 0x10000)
    {
        out += static_cast<char>(0xE0 | (codepoint >> 12));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    else
    {
        out += static_cast<char>(0xF0 | (codepoint >> 18));
        out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    return out;
}

int tui::PanelInput::utf8_char_len(unsigned char lead)
{
    if (lead < 0x80)
        return 1;
    if ((lead >> 5) == 0x6)
        return 2;
    if ((lead >> 4) == 0xE)
        return 3;
    if ((lead >> 3) == 0x1E)
        return 4;
    return 1;
}

int tui::PanelInput::Utf8Count(const std::string &text)
{
    int count = 0;
    size_t i = 0;
    while (i < text.size())
    {
        size_t step = static_cast<size_t>(utf8_char_len(static_cast<unsigned char>(text[i])));
        if (i + step > text.size())
        {
            ++count;
            break;
        }
        i += step;
        ++count;
    }
    return count;
}

std::string tui::PanelInput::JoinInputString(const std::vector<std::string> &v, const std::string &delim)
{
    std::ostringstream oss;

    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i > 0) oss << delim;
        oss << v[i];
    }

    return oss.str();
}