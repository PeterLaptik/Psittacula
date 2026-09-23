#include "panel.h"
#include <iostream>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

void tui::Panel::moveCursor(int x, int y)
{
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD position;
    position.X = static_cast<SHORT>(x);
    position.Y = static_cast<SHORT>(y);
    SetConsoleCursorPosition(console, position);
#else
    std::cout << "\033[" << (y + 1) << ';' << (x + 1) << 'H';
#endif
}

void tui::Panel::SetDimensions(int anchor_x, int anchor_y, int width, int heigth)
{
    m_anchor_x = anchor_x;
    m_anchor_y = anchor_y;
    m_width = width;
    m_height = heigth;
}
