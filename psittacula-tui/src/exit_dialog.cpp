#include "exit_dialog.h"
#include <iostream>
#include <string>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

tui::ExitDialog::ExitDialog(Screen &screen, Keyboard &keyboard)
    : m_screen(screen)
    , m_keyboard(keyboard)
{ }

// Handles exit menu
bool tui::ExitDialog::Confirm()
{
    int scr_width, scr_height;
    m_screen.GetSize(scr_width, scr_height);

    int scr_center_x = scr_width / 2;
    int scr_center_y = scr_height / 2 - 4;

    const std::string question = "Are you sure want to quit?";

    std::cout << "\x1b[?1049h" << std::flush;

    bool selected_yes = false;
    bool do_exit = false;
    bool is_screen_updated = true;
    while (true)
    {
        if (is_screen_updated)
        {
            std::cout << "\x1b[H\x1b[2J";

            for (int i = 0; i < scr_center_y - 4; i++)
                std::cout << std::endl;

            int margin_q = scr_center_x - question.length() / 2;
            for (int i = 0; i < margin_q; i++)
                std::cout << ' ';

            std::cout << question << std::endl;

            int margin_a = scr_center_x - 14 / 2;
            for (int i = 0; i < margin_a; i++)
                std::cout << ' ';

            if (selected_yes)
                std::cout << "\x1b[7m  Yes  \x1b[0m   No ";
            else
                std::cout << "  Yes   \x1b[7m  No  \x1b[0m";

            std::cout << std::endl << std::endl;
            int margin_n = scr_center_x - 56 / 2;
            for (int i = 0; i < margin_n; i++)
                std::cout << ' ';

            std::cout << "y - quit, n/Esc - stay, arrows + Enter to choose" << std::flush;
            std::cout << std::endl << std::endl;

            is_screen_updated = false;

#ifdef _WIN32
            HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
            COORD position;
            position.X = static_cast<SHORT>(0);
            position.Y = static_cast<SHORT>(0);
            SetConsoleCursorPosition(console, position);
#else
            std::cout << "\033[" << (0 + 1) << ';' << (0 + 1) << 'H';
#endif
        }

        int key = m_keyboard.ReadKey();

        if (key == Keyboard::Keys::nothing)
            continue;

        if (key == 'y' || key == 'Y')
        {
            do_exit = true;
            break;
        }
        if (key == 'n' || key == 'N' || key == 27 || key == 3 || key == 4)
        {
            do_exit = false;
            break;
        }
        if (key == Keyboard::Keys::keyLeft || key == Keyboard::Keys::keyRight)
        {
            is_screen_updated = true;
            selected_yes = !selected_yes;
            continue;
        }
        if (key == Keyboard::Keys::keyEnter)
        {
            do_exit = selected_yes;
            break;
        }
    }

    std::cout << "\x1b[?1049l" << std::flush;
    return do_exit;
}
