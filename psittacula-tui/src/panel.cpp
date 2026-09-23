#include "panel.h"
#include <iostream>
#include <cstdio>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

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
    struct termios original = {};
    if (!isatty(STDOUT_FILENO) || tcgetattr(STDIN_FILENO, &original) != 0)
        return;

    struct termios query = original;
    query.c_lflag &= ~(ECHO | ICANON);
    query.c_cc[VMIN] = 0;
    query.c_cc[VTIME] = 5;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &query) != 0)
        return;

    std::cout << "\033[6n" << std::flush;

    char reply[32] = {};
    size_t used = 0;
    while (used + 1 < sizeof(reply))
    {
        char ch = 0;
        if (read(STDIN_FILENO, &ch, 1) != 1)
            break;
        reply[used++] = ch;
        if (ch == 'R')
            break;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &original);

    int row = 0;
    int col = 0;
    if (sscanf(reply, "\033[%d;%dR", &row, &col) == 2)
    {
        x = col - 1;
        y = row - 1;
    }
#endif
}

void tui::Panel::SetDimensions(int anchor_x, int anchor_y, int width, int heigth)
{
    m_anchor_x = anchor_x;
    m_anchor_y = anchor_y;
    m_width = width;
    m_height = heigth;
}
