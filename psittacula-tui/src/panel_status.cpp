#include "panel_status.h"
#include <iostream>

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
    std::string status_text_line = m_status.size() < m_width - 2 ? 
        m_status : m_status.substr(0, m_width - 2);

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
