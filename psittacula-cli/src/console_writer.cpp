#include "console_writer.h"
#include "console_history.h"
#include "chunk_processor.h"
#include <iostream>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#endif

const char *const kDefault = "\033[0m";
const char *const kGreen = "\033[32m";
const char *const kRed = "\033[31m";
const char *const kCyan = "\033[36m";
const char *const kGrey = "\033[90m";

static console::TextReceiver *text_receiver = nullptr;
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
        case TextOrigin::splitter:
            result = kGrey;
            break;
        default:
            result = kDefault;
    }
    return result;
}

void console::set_up_console(TextReceiver *receiver)
{
    if (receiver)
    {
        text_receiver = receiver;
        return;
    }
    else
    {
        text_receiver = nullptr;
    }

#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

console::TextReceiver* console::get_current_receiver()
{
    return text_receiver;
}

console::LineInputScope::LineInputScope()
{
#ifdef _WIN32
    HANDLE h_in = GetStdHandle(STD_INPUT_HANDLE);
    if (h_in == INVALID_HANDLE_VALUE || h_in == nullptr)
        return;
    DWORD mode = 0;
    if (!GetConsoleMode(h_in, &mode))
        return;
    m_handle = static_cast<void*>(h_in);
    m_mode = static_cast<unsigned long>(mode);
    // Restore cooked input: line assembly, echo and CR->LF translation
    DWORD cooked = mode | (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    cooked &= ~static_cast<DWORD>(ENABLE_WINDOW_INPUT);
    SetConsoleMode(h_in, cooked);
    FlushConsoleInputBuffer(h_in);
    m_saved = true;
#else
    m_fd = STDIN_FILENO;
    if (tcgetattr(m_fd, &m_termios) != 0)
    {
        m_fd = -1;
        return;
    }
    struct termios cooked = m_termios;
    cooked.c_lflag |= (ECHO | ICANON | ISIG);
    tcsetattr(m_fd, TCSANOW, &cooked);
    tcflush(m_fd, TCIFLUSH);
    m_saved = true;
#endif
    // Drop any stale input buffered while in raw mode, so the first
    // getline starts with a clean line
    std::cin.clear();
    std::cin.sync();
}

console::LineInputScope::~LineInputScope()
{
#ifdef _WIN32
    if (m_saved && m_handle != nullptr)
        SetConsoleMode(static_cast<HANDLE>(m_handle), static_cast<DWORD>(m_mode));
#else
    if (m_saved && m_fd >= 0)
        tcsetattr(m_fd, TCSANOW, &m_termios);
#endif
}

void console::write_line(const std::string &message, TextOrigin origin)
{
    if (text_receiver)
    {
        text_receiver->WriteLine(message, origin);
        return;
    }

    const char *colour = get_origin_colour(origin);
    std::cout << colour << message << std::endl;
    history.AddLine(colour + message);
}

void console::write(const std::string &message, TextOrigin origin)
{
    if (text_receiver)
    {
        text_receiver->Write(message, origin);
        return;
    }

    const char *colour = get_origin_colour(origin);
    std::cout << colour << message;
    history.AddText(colour + message);
}

void console::write_splitter(TextOrigin origin)
{
    if (text_receiver)
    {
        text_receiver->WriteLine("---------------------------------------------------", origin);
        return;
    }

    const char *colour = get_origin_colour(origin);
    std::cout << colour << "---------------------------------------------------" << std::endl;
}

void console::write_status(const ChunkProcessor *proc, int context_size)
{
    int total, completion, prompt;
    double cost;
    proc->GetTokensStat(total, completion, prompt, cost);

    double ratio_ctx = context_size > 0 ? static_cast<double>(total) / static_cast<double>(context_size) : 0;
    double percentage_ctx = std::round(ratio_ctx * 10000) / 100;

    std::ostringstream bar;

    if (text_receiver)
    {
        // Add percentage of context graphic view
        const int bar_width = 30;
        int filled_blocks = static_cast<int>(bar_width * ratio_ctx);
        
        bar << "   [";
        for (int i = 0; i < filled_blocks; ++i)
        {
            bar << "\xE2\x96\x93";
        }
        for (int i = filled_blocks; i < bar_width; ++i)
        {
            bar << "\xE2\x96\x91";
        }
        bar << "] ";
        bar << " " << std::fixed << std::setprecision(2) << percentage_ctx << "%    ";
        bar << "tokens: " << std::to_string(total);
        bar << " (prompt: " << std::to_string(prompt);
        bar << " / completion: " << std::to_string(completion) << ")";

        if (cost > 0)
            bar << "    cost: " << std::to_string(cost);

        text_receiver->RefreshStatus(bar.str());
    }
    else
    {
        bar << std::fixed << std::setprecision(2) << percentage_ctx;
        std::string percentage_ctx_str = bar.str();

        // context_size, m_total_tokens
        std::string context_usage_str = context_size > 0 ?
            percentage_ctx_str + "% of context (" + std::to_string(context_size) + ")" : "";

        console::write_line("\nTokens: " + std::to_string(total)
            + " (prompt: " + std::to_string(prompt) +
            +" / completion: " + std::to_string(completion)
            + ") \t"
            + context_usage_str
            + (cost > 0 ? "cost: " + std::to_string(cost) : "")
            + "\n", console::TextOrigin::reasoning);
        console::write_splitter();
    }
}

void console::flush()
{
    if (text_receiver)
    {
        return;
    }

    std::cout << std::flush;
}

void console::clear()
{
    if (text_receiver)
    {
        text_receiver->Clear();
        return;
    }

    std::cout << "\033[2J\033[1;1H";
}

void console::move_spinner()
{
    if (text_receiver)
    {
        text_receiver->MoveSpinner();
    }
}
