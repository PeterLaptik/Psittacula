#include "screen.h"
#include <iostream>
#include <clocale>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <csignal>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

// Shared with keyboard.cpp: set by SIGWINCH, polled in ReadKey().
volatile sig_atomic_t g_resize_pending = 0;
extern "C" void handle_winch(int sig)
{
    (void)sig;
    g_resize_pending = 1;
}
#endif

tui::Screen::Screen()
    : m_panel_input(static_cast<Panel*>(this)), 
    m_panel_text(static_cast<Panel *>(this))
{ }

void tui::Screen::Show()
{
    SetUpScreen();
    UpdateSize();

    bool screen_result = CheckScreen();
    if (!screen_result)
        return;

    DrawFrame();

#ifndef _WIN32
    EnableRawMode();

    struct sigaction sa {};
    sa.sa_handler = handle_winch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, nullptr);
#endif
}

tui::Response tui::Screen::PutChar(int key)
{
    return m_panel_input.PutChar(key);
}

void tui::Screen::PutText(const std::string &txt, TextOrigin origin)
{
    m_panel_text.AddText(txt, origin);
}

void tui::Screen::Clear()
{
    m_panel_text.Clear();
}

void tui::Screen::UpdateFilesAutocompleteList(std::vector<std::string> &files)
{
    m_panel_input.UpdateFilesAutocompleteList(files);
}

void tui::Screen::UpdateCommandsAutocompleteList(std::vector<std::string> &commands)
{
    m_panel_input.UpdateCommandsAutocompleteList(commands);
}

void tui::Screen::OnUpdatedChild(Panel *updated_panel)
{
    DrawFrame();
}

void tui::Screen::SetUpScreen()
{
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode))
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD inMode = 0;
    if (hIn != INVALID_HANDLE_VALUE && hIn != nullptr &&
        GetConsoleMode(hIn, &inMode))
    {
        inMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
        inMode |= ENABLE_WINDOW_INPUT;
        SetConsoleMode(hIn, inMode);
    }
#else
    // POSIX terminals: UTF-8
    setlocale(LC_ALL, "");
#endif
}

bool tui::Screen::CheckScreen()
{
    if (m_props.columns <= 0 || m_props.rows <= 2)
    {
        std::cerr << "Wrong screen size (too small):" 
            << " cols = " << m_props.columns 
            << " rows = " << m_props.rows 
            << std::endl;
        return false;
    }
    return true;
}

void tui::Screen::UpdateSize()
{
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (GetConsoleScreenBufferInfo(console, &info))
    {
        m_props.columns = info.srWindow.Right - info.srWindow.Left + 1;
        m_props.rows = info.srWindow.Bottom - info.srWindow.Top;
    }
#else
    winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
    {
        m_props.columns = ws.ws_col;
        m_props.rows = ws.ws_row;
    }
#endif
}

bool tui::Screen::HandleResize()
{
    int old_columns = m_props.columns;
    int old_rows = m_props.rows;

#ifndef _WIN32
    g_resize_pending = 0;
#endif

    UpdateSize();

    if (m_props.columns == old_columns && m_props.rows == old_rows)
        return false;

    ClearScreen(old_columns, old_rows);
    m_panel_input.UpdateSize(m_props.columns, m_props.rows);
    m_panel_text.UpdateSize(m_props.columns, m_props.rows);

    DrawFrame();
    return true;
}

void tui::Screen::GetSize(int &x, int &y) const
{
    x = m_props.columns;
    y = m_props.rows;
}

void tui::Screen::ClearScreen(int columns, int rows)
{
    moveCursor(0, 0);
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            std::cout << ' ';
        }
    }
}

void tui::Screen::DrawFrame()
{
    int input_rows = m_panel_input.GetInputRowsNumber() + 1; // all input + one current input line
    int text_height = m_props.rows - input_rows - 1;
    int text_width = m_props.columns;
    if (text_height > 0)
    {
        m_panel_text.SetDimensions(0, 0, text_width, text_height);
        m_panel_text.Draw();
    }

    int input_height = input_rows;
    int input_width = m_props.columns;
    m_panel_input.SetDimensions(0, m_props.rows - input_rows, input_width, input_height);
    m_panel_input.Draw();
}

#ifndef _WIN32
void tui::Screen::EnableRawMode()
{
    tcgetattr(STDIN_FILENO, &g_original_termios);
    struct termios raw = g_original_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void tui::Screen::DisableRawMode()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
}
#endif
