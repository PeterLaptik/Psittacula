#include "screen.h"
#include "keyboard.h"
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

namespace {
    // Cooked-mode terminal saved by EnableRawMode. A copy is kept at file
    // scope because the fatal-signal handler cannot reach class members.
    struct termios g_signal_saved_termios {};
    bool g_signal_saved_valid = false;

    struct sigaction g_saved_sigint {}, g_saved_sigterm {}, g_saved_sighup {};
    bool g_fatal_handlers_installed = false;
}

// SIGINT / SIGTERM / SIGHUP: restore the terminal before dying, then let the
// default disposition kill the process with the status the shell expects.
// Only async-signal-safe calls: tcsetattr, write, sigaction, raise.
extern "C" void RestoreConsoleOnFatalSignal(int sig)
{
    if (g_signal_saved_valid)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_signal_saved_termios);
        g_signal_saved_valid = false;
    }

    // Make the cursor visible and leave the alternate screen, if still shown
    const char reset[] = "\x1b[?25h\x1b[?1049l";
    if (write(STDOUT_FILENO, reset, sizeof(reset) - 1) == -1)
    {
        // The console may already be gone: nothing to report here
    }

    struct sigaction dfl {};
    dfl.sa_handler = SIG_DFL;
    sigemptyset(&dfl.sa_mask);
    sigaction(sig, &dfl, nullptr);

    raise(sig); // default action terminates the process
}

void InstallFatalSignalHandlers()
{
    struct sigaction sa {};
    sa.sa_handler = RestoreConsoleOnFatalSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, &g_saved_sigint);
    sigaction(SIGTERM, &sa, &g_saved_sigterm);
    sigaction(SIGHUP, &sa, &g_saved_sighup);

    g_fatal_handlers_installed = true;
}

void UninstallFatalSignalHandlers()
{
    if (!g_fatal_handlers_installed)
        return;

    sigaction(SIGINT, &g_saved_sigint, nullptr);
    sigaction(SIGTERM, &g_saved_sigterm, nullptr);
    sigaction(SIGHUP, &g_saved_sighup, nullptr);

    g_fatal_handlers_installed = false;
    g_signal_saved_valid = false;
}
#endif

tui::Screen::Screen()
    : m_panel_input(static_cast<Panel*>(this)), 
    m_panel_text(static_cast<Panel *>(this))
{ }

tui::Screen::~Screen()
{
#ifndef _WIN32
    // Restore the original fatal dispositions first (no handler of ours can
    // run after that), then the terminal itself. No-ops when Show() never
    // got to enable anything.
    UninstallFatalSignalHandlers();
    DisableRawMode();
#else
    RestoreInputMode();
#endif
    std::cout << "\x1b[?25h" << std::flush; // cursor visible for whoever comes next
}

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
    InstallFatalSignalHandlers();

    struct sigaction sa {};
    sa.sa_handler = handle_winch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, nullptr);
#endif
}

tui::Response tui::Screen::PutChar(int key)
{
    std::lock_guard guard(m_screen_locker);

    if (key == Keyboard::Keys::keyPageUp)
    {
        m_panel_text.ScrollPageUp();
        return Response();
    }

    if (key == Keyboard::Keys::keyPageDown)
    {
        m_panel_text.ScrollPageDown();
        return Response();
    }

    // Arrow keys scroll the text unless the autocomplete list consumes them
    if (key == Keyboard::Keys::keyUp && !m_panel_input.IsAutocompleteActive())
    {
        m_panel_text.ScrollUp(1);
        return Response();
    }

    if (key == Keyboard::Keys::keyDown && !m_panel_input.IsAutocompleteActive())
    {
        m_panel_text.ScrollDown(1);
        return Response();
    }

    return m_panel_input.PutChar(key);
}

void tui::Screen::PutText(const std::string &txt, TextOrigin origin)
{
    std::lock_guard guard(m_screen_locker);
    m_panel_text.AddText(txt, origin);
}

void tui::Screen::SetStatusLine(const std::string &status)
{
    std::lock_guard guard(m_screen_locker);
    m_panel_status.SetStatus(status);
}

void tui::Screen::MoveSpinner()
{
    std::lock_guard guard(m_screen_locker);
    m_panel_status.MoveSpinner();
}

void tui::Screen::Clear()
{
    std::lock_guard guard(m_screen_locker);
    m_panel_text.Clear();
}

void tui::Screen::SetShowReasoning(bool reasoning)
{

    std::lock_guard<std::mutex> guard(m_screen_locker);
    m_panel_text.SetShowReasoning(reasoning);
}

bool tui::Screen::GetShowReasoning() const
{
    std::lock_guard<std::mutex> guard(m_screen_locker);
    return m_panel_text.GetShowReasoning();
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
    // Save everything about to be changed: the destructor restores it later
    m_saved_cp_input = GetConsoleCP();
    m_saved_cp_output = GetConsoleOutputCP();

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr &&
        GetConsoleMode(hOut, &dwMode))
    {
        m_saved_output_mode = dwMode;
        m_output_saved = true;
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD inMode = 0;
    if (hIn != INVALID_HANDLE_VALUE && hIn != nullptr &&
        GetConsoleMode(hIn, &inMode))
    {
        m_saved_input_mode = inMode;
        m_input_saved = true;

        inMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
        inMode |= ENABLE_WINDOW_INPUT;
        SetConsoleMode(hIn, inMode);
    }

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#else
    // POSIX terminals: UTF-8 locale for input decoding / formatting
    setlocale(LC_ALL, "");
#endif
}

#ifdef _WIN32
void tui::Screen::RestoreInputMode()
{
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE && hIn != nullptr && m_input_saved)
        SetConsoleMode(hIn, m_saved_input_mode);

    // VT processing is sticky per-process in some hosts: restore the whole
    // saved output mode, not just the VT bit
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr && m_output_saved)
        SetConsoleMode(hOut, m_saved_output_mode);

    // console code pages: 0 means there was no console to query at startup
    if (m_saved_cp_input != 0)
        SetConsoleCP(m_saved_cp_input);
    if (m_saved_cp_output != 0)
        SetConsoleOutputCP(m_saved_cp_output);
}
#endif

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
        m_props.rows = info.srWindow.Bottom - info.srWindow.Top + 1;
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
    std::lock_guard guard(m_screen_locker);

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
    MoveCursorTo(0, 0);
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
    HideCursor(true);
    int input_rows = m_panel_input.GetInputRowsNumber() + 1; // all input + one current input line
    int text_height = m_props.rows - input_rows - 2; // substract border + status panel size
    int text_width = m_props.columns;

    m_panel_status.SetDimensions(0, 0, text_width, 1);
    m_panel_status.Draw();

    if (text_height > 0)
    {
        m_panel_text.SetDimensions(0, 1, text_width, text_height);
        m_panel_text.Draw();
    }

    int input_height = input_rows;
    int input_width = m_props.columns;
    m_panel_input.SetDimensions(0, m_props.rows - input_rows - 1, input_width, input_height);
    m_panel_input.Draw();
    HideCursor(false);
}

#ifndef _WIN32
void tui::Screen::EnableRawMode()
{
    if (tcgetattr(STDIN_FILENO, &g_original_termios) != 0)
        return; // not a tty: nothing we could restore later either

    struct termios raw = g_original_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    // Without this, the tty driver translates the CR (13) sent by Enter into
    // NL (10) before read() sees it, so keyEnter (13) never matches downstream.
    raw.c_iflag &= ~(ICRNL | INLCR | IXON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
        return; // failed to switch: do not claim raw mode

    m_raw_mode_active = true;

    // A second copy for the fatal-signal handler, which cannot reach
    // class members
    g_signal_saved_termios = g_original_termios;
    g_signal_saved_valid = true;
}

void tui::Screen::DisableRawMode()
{
    if (!m_raw_mode_active)
        return;

    tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
    m_raw_mode_active = false;
    g_signal_saved_valid = false;
}
#endif
