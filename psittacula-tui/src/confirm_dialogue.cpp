#include "confirm_dialogue.h"
#include "keyboard.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <vector>

namespace tui {

    namespace {
        // Upper bound for the dialog: after this it denies itself,
        // so a stuck UI can never park the engine thread forever
        constexpr std::chrono::minutes kConfirmTimeout{5};

        // Trims the line so it fits into the confirm frame
        std::string FitToWidth(const std::string &line, int width)
        {
            if (width <= 0)
                return "";
            if (static_cast<int>(line.length()) > width)
                return line.substr(0, static_cast<size_t>(width));
            return line;
        }

        // Prints margin spaces followed by the row body
        void PrintIndented(int margin, const std::string &line)
        {
            for (int i = 0; i < margin; i++)
                std::cout << ' ';
            std::cout << line << std::endl;
        }
    }

    ConfirmDialogue::ConfirmDialogue(Screen &screen)
        : m_screen(screen)
    { }

    bool ConfirmDialogue::Confirm(const std::string &message)
    {
        int scr_width = 0;
        int scr_height = 0;
        m_screen.GetSize(scr_width, scr_height);

        m_is_shown = true;

        // The message can span lines: each line gets its own frame row
        std::vector<std::string> lines;
        {
            std::istringstream iss(message);
            std::string line;
            while (std::getline(iss, line))
                lines.push_back(line);
            if (lines.empty())
                lines.emplace_back();
        }

        const std::string title = "Confirm the action?";
        const std::string hint = "y - allow,  n/Esc - deny";

        int content_width = static_cast<int>(title.length());
        for (const std::string &line : lines)
            content_width = std::max(content_width, static_cast<int>(line.length()));

        content_width = std::min(content_width, std::max(1, scr_width - 6));

        const std::string border = "+" + std::string(static_cast<size_t>(content_width) + 2, '-') + "+";
        int box_width = content_width + 4;
        int box_margin = std::max(0, (scr_width - box_width) / 2);
        int hint_margin = std::max(0, (scr_width - static_cast<int>(hint.length())) / 2);

        int block_height = static_cast<int>(lines.size()) + 6; // borders, title, blank rows, hint
        int top_margin = std::max(0, (scr_height - block_height) / 2);

        std::cout << "\x1b[?1049h" << std::flush;
        m_screen.HideCursor(true);

        for (int i = 0; i < top_margin; i++)
            std::cout << std::endl;

        const std::string blank_content(static_cast<size_t>(content_width), ' ');
        const std::string blank_row(static_cast<size_t>(box_width) - 2, ' ');

        PrintIndented(box_margin, border);
        PrintIndented(box_margin, "| " + FitToWidth(title, content_width) + " |");
        PrintIndented(box_margin, "| " + blank_content + " |");
        for (const std::string &line : lines)
            PrintIndented(box_margin, "| " + FitToWidth(line, content_width) + " |");
        PrintIndented(box_margin, "|" + blank_row + "|");
        PrintIndented(box_margin, border);

        PrintIndented(hint_margin, hint);
        std::cout << std::flush;

        bool confirmed_result = false;
        std::unique_lock<std::mutex> lock(m_key_guard);

        m_pressed_key = 0;

        // wait_for: spurious wakeups handled by the predicate; on timeout the
        // dialog denies itself instead of parking the worker forever
        m_cv.wait_for(lock, kConfirmTimeout, [this] {
                return m_pressed_key == 'y' || m_pressed_key == 'Y' ||
                    m_pressed_key == 'n' || m_pressed_key == 'N' ||
                    m_pressed_key == 27 || m_pressed_key == Keyboard::Keys::eof;
            }
        );

        if (m_pressed_key == 'y' || m_pressed_key == 'Y')
        {
            confirmed_result = true;
        }

        lock.unlock();

        m_screen.HideCursor(false);
        std::cout << "\x1b[?1049l" << std::flush;
        m_is_shown = false;
        return confirmed_result;
    }

    void ConfirmDialogue::PutCharFromKeyboard(int key)
    {
        {
            std::lock_guard<std::mutex> lock(m_key_guard);
            m_pressed_key = key;
        }
        m_cv.notify_one();
    }
    bool ConfirmDialogue::IsShown() const
    {
        return m_is_shown;
    }

    void ConfirmDialogue::Cancel()
    {
        PutCharFromKeyboard(Keyboard::Keys::eof);
    }
}