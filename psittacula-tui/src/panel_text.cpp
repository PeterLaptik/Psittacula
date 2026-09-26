#include "panel_text.h"
#include <iostream>
#include <string>
#include <vector>

namespace {
    int utf8_char_len(unsigned char lead)
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

    // Visible cell count: UTF-8 code points minus ANSI "\033[...m" sequences.
    int DisplayWidth(const std::string &text)
    {
        int width = 0;
        for (size_t i = 0; i < text.size();)
        {
            if (text[i] == '\033' && i + 1 < text.size() && text[i + 1] == '[')
            {
                size_t end = text.find('m', i + 2);
                if (end == std::string::npos)
                    break;
                i = end + 1;
                continue;
            }
            size_t step = static_cast<size_t>(utf8_char_len(static_cast<unsigned char>(text[i])));
            if (i + step > text.size())
            {
                ++width;
                break;
            }
            i += step;
            ++width;
        }
        return width;
    }
}

tui::PanelText::PanelText(Panel *parent)
    : Panel(parent)
{ }

void tui::PanelText::AddText(const std::string & txt, TextOrigin origin)
{
    m_text_content.AddText(txt, origin);
    m_view_shift = 0;
    Refresh();
}

void tui::PanelText::Draw()
{
    if (m_width <= 0 || m_height <= 0)
        return;

    int actual_line_length = m_width - kTextPaddingLeft - kTextPaddingRight;
    if (actual_line_length < 1)
        actual_line_length = 1;
    if (m_text_content.GetMaxLineLength() != actual_line_length)
        m_text_content.SetMaxLineLength(actual_line_length);

    for (int row = 0; row <= m_height; ++row)
    {
        bool top = row == 0;
        bool bottom = row == m_height;

        std::string line;
        line.reserve(static_cast<size_t>(m_width));
        for (int col = 0; col < m_width; ++col)
        {
            bool left = col == 0;
            bool right = col == m_width - 1;

            char ch = ' ';
            if ((top || bottom) && (left || right))
                ch = '+';
            else if (top || bottom)
                ch = '-';
            else if (left || right)
                ch = '|';
            line.push_back(ch);
        }

        MoveCursorTo(m_anchor_x, m_anchor_y + row);
        std::cout << "\033[34m" << line << "\033[0m";
    }
    std::cout.flush();
    Refresh();
}

void tui::PanelText::Clear()
{
    m_text_content.Clear();
    m_view_shift = 0;
    Refresh();
}

void tui::PanelText::Refresh()
{
    HideCursor(true);
    int cursor_old_x, cursor_old_y;
    GetCursorPosition(cursor_old_x, cursor_old_y);

    int x = m_anchor_x + kTextPaddingLeft;
    int y = m_anchor_y + 1;
    int w = m_width - kTextPaddingLeft - kTextPaddingRight;
    int h = m_height - 2;

    if (w <= 0 || h <= 0)
        return;

    std::vector<std::string> acc;
    acc.reserve(static_cast<size_t>(h));
    m_text_content.GetTextWindowForHeight(h, m_view_shift, acc);

    for (int line_num = 0; line_num < h; ++line_num)
    {
        MoveCursorTo(x, y + line_num);
        if (line_num < static_cast<int>(acc.size()))
        {
            const std::string &line = acc[static_cast<size_t>(line_num)];
            std::cout << line;
            int pad = w - DisplayWidth(line);
            if (pad > 0)
                std::cout << std::string(static_cast<size_t>(pad), ' ');
        }
        else
        {
            std::cout << std::string(static_cast<size_t>(w), ' ');
        }
    }
    std::cout.flush();

    MoveCursorTo(cursor_old_x, cursor_old_y);
    HideCursor(false);
}

void tui::PanelText::UpdateSize(int width, int height)
{
    m_width = width;
    m_height = height;

    int actual_line_length = width - kTextPaddingLeft - kTextPaddingRight;
    if (actual_line_length < 1)
        actual_line_length = 1;
    if (m_text_content.GetMaxLineLength() != actual_line_length)
        m_text_content.SetMaxLineLength(actual_line_length);

    int max_shift = MaxViewShift();
    if (m_view_shift > max_shift)
        m_view_shift = max_shift;

    NotifyParentAboutChanges();
}

void tui::PanelText::SetShowReasoning(bool reasoning)
{
    m_text_content.SetShowReasoning(reasoning);
    NotifyParentAboutChanges();
}

bool tui::PanelText::GetReasoning() const
{
    return m_text_content.GetShowReasoning();
}

void tui::PanelText::ScrollUp(int lines)
{
    if (lines <= 0)
        return;

    int max_shift = MaxViewShift();
    if (m_view_shift >= max_shift)
        return;

    m_view_shift += lines;
    if (m_view_shift > max_shift)
        m_view_shift = max_shift;

    Refresh();
}

void tui::PanelText::ScrollDown(int lines)
{
    if (lines <= 0 || m_view_shift <= 0)
        return;

    m_view_shift -= lines;
    if (m_view_shift < 0)
        m_view_shift = 0;

    Refresh();
}

void tui::PanelText::ScrollPageUp()
{
    ScrollUp(TextWindowHeight());
}

void tui::PanelText::ScrollPageDown()
{
    ScrollDown(TextWindowHeight());
}

int tui::PanelText::TextWindowHeight() const
{
    int h = m_height - 2;
    return h > 0 ? h : 1;
}

int tui::PanelText::MaxViewShift() const
{
    int max_shift = m_text_content.GetRenderedLinesCount() - TextWindowHeight() + 1;
    return max_shift > 0 ? max_shift : 0;
}
