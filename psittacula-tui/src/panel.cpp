#include "panel.h"
#include <iostream>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#endif

namespace {
    // Tracks the terminal cursor position as last set via Panel::MoveCursorTo().
    int g_tracked_cursor_x = 0;
    int g_tracked_cursor_y = 0;
}

void tui::Panel::MoveCursorTo(int x, int y)
{
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD position;
    position.X = static_cast<SHORT>(x);
    position.Y = static_cast<SHORT>(y);
    SetConsoleCursorPosition(console, position);
#else
    std::cout << "\033[" << (y + 1) << ';' << (x + 1) << 'H';
    g_tracked_cursor_x = x;
    g_tracked_cursor_y = y;
#endif
}

void tui::Panel::GetCursorPosition(int &x, int &y)
{
    x = 0;
    y = 0;

#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(console, &info))
    {
        x = info.dwCursorPosition.X;
        y = info.dwCursorPosition.Y;
    }
#else
    // "\033[row;colR" reply from stdin vs Keyboard::ReadKey() checks
    x = g_tracked_cursor_x;
    y = g_tracked_cursor_y;
#endif
}

void tui::Panel::HideCursor(bool hide)
{
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(handle, &info);
    info.bVisible = !hide;
    SetConsoleCursorInfo(handle, &info);
#else
    std::string sequence = hide ? "\033[?25l" : "\033[?25h";
    std::cout << sequence << std::flush;
#endif
}

void tui::Panel::SetDimensions(int anchor_x, int anchor_y, int width, int heigth)
{
    m_anchor_x = anchor_x;
    m_anchor_y = anchor_y;
    m_width = width;
    m_height = heigth;
}
