#include "panel_status.h"
#include <iostream>

static size_t spinner_cursor = 0;
static const char spinner[4] = { '/', '|', '\\', '-' };
static size_t spinner_clr_cursor = 0;
static const std::string spinner_clr[7] = { "\033[36m", "\033[33m", "\033[31m", "\033[32m", "\033[35m", "\033[34m", "\033[37m" };

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

    int utf8_count(const std::string &text)
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
}

tui::PanelStatus::PanelStatus(Panel *parent)
    : Panel(parent)
{
    m_height = 1;
}

void tui::PanelStatus::SetStatus(const std::string &status)
{
    m_status = status;
    Refresh();
    // No need full redraw
    //NotifyParentAboutChanges();
}

void tui::PanelStatus::Clear()
{
    m_status = "";
    Refresh();
    // No need full redraw
    // NotifyParentAboutChanges();
}

void tui::PanelStatus::Refresh()
{
    int char_limit = m_width -3;
    int char_count = utf8_count(m_status);

    std::string status_text_line = m_status;
    if (char_count > char_limit)
    {
        int byte_limit = 0;
        size_t i = 0;
        int count = 0;
        while (i < m_status.size() && count < char_limit)
        {
            size_t step = static_cast<size_t>(utf8_char_len(static_cast<unsigned char>(m_status[i])));
            if (i + step > m_status.size())  {
                break;
            }
            byte_limit += step;
            i += step;
            ++count;
        }
        status_text_line = m_status.substr(0, byte_limit);
    }

    int cx, cy;
    GetCursorPosition(cx, cy);
    MoveCursorTo(m_anchor_x + 1, m_anchor_y);
    std::cout << status_text_line;
    std::cout.flush();
    MoveCursorTo(cx, cy);
}

void tui::PanelStatus::Draw()
{
    if (m_width <= 0 || m_height <= 0)
        return;

    for (int row = 0; row < m_height; ++row)
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
            if (left || right)
                ch = '|';
            line.push_back(ch);
        }

        MoveCursorTo(m_anchor_x, m_anchor_y + row);
        std::cout << "\033[34m" << line << "\033[0m";
    }

    MoveCursorTo(m_anchor_x + 1, m_anchor_y);
    std::cout << m_status;
    std::cout.flush();
}

void tui::PanelStatus::UpdateSize(int width, int height)
{
    m_width = width;
    m_height = 1;

    NotifyParentAboutChanges();
}

void tui::PanelStatus::MoveSpinner()
{
    static std::string spinner_txt;

    spinner_txt.clear();
    spinner_cursor = (spinner_cursor >= 3 ? 0 : spinner_cursor + 1);
    spinner_clr_cursor = (spinner_clr_cursor >= 6 ? 0 : spinner_clr_cursor + 1);

    spinner_txt = spinner_clr[spinner_clr_cursor] + spinner[spinner_cursor] + "\033[0m";

    int cx, cy;
    GetCursorPosition(cx, cy);
    MoveCursorTo(m_anchor_x + 2, m_anchor_y);
    std::cout << spinner_txt;
    std::cout.flush();
    MoveCursorTo(cx, cy);
}
