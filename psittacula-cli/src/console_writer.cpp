#include "console_writer.h"
#include "console_history.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

const char *const kDefault = "\033[0m";
const char *const kGreen = "\033[32m";
const char *const kRed = "\033[31m";
const char *const kCyan = "\033[36m";
const char *const kGrey = "\033[90m";

static ConsoleHistory history;

const char* console::get_origin_colour(TextOrigin origin)
{
    const char *result = kDefault;
    switch (origin)
    {
        case TextOrigin::machine:
            result = kGreen;
            break;
        case TextOrigin::error:
            result = kRed;
            break;
        case TextOrigin::filesystem:
            result = kCyan;
            break;
        case TextOrigin::reasoning:
            result = kGrey;
            break;
        default:
            result = kDefault;
    }
    return result;
}

void console::set_up_console()
{
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

void console::write_line(const std::string &message, TextOrigin origin)
{
    const char *colour = get_origin_colour(origin);
    std::cout << colour << message << std::endl;
    history.AddLine(colour + message);
}

void console::write(const std::string &message, TextOrigin origin)
{
    const char *colour = get_origin_colour(origin);
    std::cout << colour << message;
    history.AddText(colour + message);
}

void console::flush()
{
    std::cout << std::flush;
}

void console::clear()
{
    std::cout << "\033[2J\033[1;1H";
}

void console::show_history()
{
    auto history_text = history.GetHistory();
    for (const auto &line : history_text)
    {
        std::cout << line << std::endl;
    }
}
