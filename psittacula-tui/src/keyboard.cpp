#include "keyboard.h"

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
// Old MinGW.org 5.1 lacks _getwch()/_kbhit() declarations; MSVC/MinGW-w64
// already declare them (redeclaring would trigger C4273).
#if defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR) && !defined(_MSC_VER)
extern "C" unsigned int _getwch(void);
extern "C" int _kbhit(void);
#endif
#endif

int tui::Keyboard::ReadKey()
{
#ifdef _WIN32

    HANDLE h_in = GetStdHandle(STD_INPUT_HANDLE);
    if (h_in == INVALID_HANDLE_VALUE || h_in == nullptr)
    {
        return Keys::nothing;
    }

    DWORD mode = 0;
    bool is_console = GetConsoleMode(h_in, &mode) != 0;
    if (!is_console)
    {
        int fallback = _getwch();
        if (fallback == 0 || fallback == 0xE0)
        {
            int code = _getwch();
            if (code == 75)
                return Keys::keyLeft;
            if (code == 77)
                return Keys::keyRight;
            if (code == 83)
                return Keys::keyDel;
            if (code == 72)
                return Keys::keyUp;
            if (code == 80)
                return Keys::keyDown;
            if (code == 73)
                return Keys::keyPageUp;
            if (code == 81)
                return Keys::keyPageDown;
            return Keys::nothing;
        }
        if (fallback < 0 || fallback == 0xFFFF)
            return Keys::nothing;
        if (fallback >= 0xD800 && fallback <= 0xDFFF)
            return Keys::nothing;
        return fallback;
    }

    if (WaitForSingleObject(h_in, 50) != WAIT_OBJECT_0)
        return Keys::nothing;

    static int pending_high = -1;
    for (int drained = 0; drained < 16; ++drained)
    {
        DWORD pending = 0;
        if (!GetNumberOfConsoleInputEvents(h_in, &pending) || pending == 0)
            return Keys::nothing;

        INPUT_RECORD rec = {};
        DWORD got = 0;
        if (!ReadConsoleInputW(h_in, &rec, 1, &got) || got == 0)
            return Keys::nothing;

        if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT)
            return Keys::resize;
        if (rec.EventType == FOCUS_EVENT || rec.EventType == MENU_EVENT)
            continue;
        if (rec.EventType == MOUSE_EVENT)
            continue;
        if (rec.EventType != KEY_EVENT)
            continue;
        if (!rec.Event.KeyEvent.bKeyDown)
            continue;

        WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_DELETE || vk == VK_UP || vk == VK_DOWN ||
            vk == VK_PRIOR || vk == VK_NEXT)
            pending_high = -1; // navigation breaks a pending pair: drop it
        if (vk == VK_LEFT)
            return Keys::keyLeft;
        if (vk == VK_RIGHT)
            return Keys::keyRight;
        if (vk == VK_UP)
            return Keys::keyUp;
        if (vk == VK_DOWN)
            return Keys::keyDown;
        if (vk == VK_DELETE)
            return Keys::keyDel;
        if (vk == VK_PRIOR)
            return Keys::keyPageUp;
        if (vk == VK_NEXT)
            return Keys::keyPageDown;

        wchar_t ch = rec.Event.KeyEvent.uChar.UnicodeChar;
        if (ch == 0)
            continue;

        int key = static_cast<int>(ch);
        if (key >= 0xD800 && key <= 0xDBFF)
        {
            pending_high = key;
            continue;
        }
        if (key >= 0xDC00 && key <= 0xDFFF)
        {
            if (pending_high >= 0xD800 && pending_high <= 0xDBFF)
            {
                int combined = 0x10000 + ((pending_high - 0xD800) << 10) + (key - 0xDC00);
                pending_high = -1;
                return combined;
            }
            continue;
        }
        if (key == 0x7F) // DEL control char: some layouts report it as text
            continue;
        pending_high = -1;
        return key;
    }
    
    return Keys::nothing;

#else

    // POSIX input arrives as raw bytes, so UTF-8 sequences are read and decoded manually.
    unsigned char key = 0;
    if (read(STDIN_FILENO, &key, 1) != 1)
    {
        // A signal interrupted the read; report the resize instead of a spurious key.
        if (errno == EINTR && g_resize_pending)
            return Keys::resize;
        return Keys::eof;
    }
    if (key != 27)
    {
        // Determine how many bytes this UTF-8 character occupies from its lead byte.
        int len = utf8_char_len(key);
        if (len == 1)
            return key;

        // Read the remaining continuation bytes of the character.
        unsigned char bytes[4];
        bytes[0] = key;
        for (int i = 1; i < len; ++i)
        {
            if (read(STDIN_FILENO, &bytes[i], 1) != 1)
            {
                if (errno == EINTR && g_resize_pending)
                    return Keys::resize;
                return Keys::nothing;
            }
        }
        return Utf8Decode(bytes, len);
    }

    // ESC alone is a valid key, but it also starts an escape sequence (arrows, Delete).
    // Wait briefly for more input to tell the two apart.
    if (!InputAvailable(50))
        return g_resize_pending ? Keys::resize : 27;

    // A CSI sequence begins with ESC '['.
    unsigned char sequence = 0;
    if (read(STDIN_FILENO, &sequence, 1) != 1)
    {
        if (errno == EINTR && g_resize_pending)
            return Keys::resize;
        return Keys::nothing;
    }
    if (sequence != '[')
        return Keys::nothing;

    // Collect numeric parameters until the final byte (0x40-0x7E) ends the sequence.
    std::string params;
    unsigned char final = 0;
    while (true)
    {
        if (read(STDIN_FILENO, &final, 1) != 1)
        {
            if (errno == EINTR && g_resize_pending)
                return Keys::resize;
            return 27;
        }
        if (final >= 0x40 && final <= 0x7E)
            break;
        if (final >= '0' && final <= '9')
            params += static_cast<char>(final);
    }

    // Map the final byte (and parameter) to a known key; 0 means an unsupported sequence.
    if (final == 'A')
        return Keys::keyUp;
    if (final == 'B')
        return Keys::keyDown;
    if (final == 'D')
        return Keys::keyLeft;
    if (final == 'C')
        return Keys::keyRight;
    if (final == '~' && params == "3")
        return Keys::keyDel;
    if (final == '~' && params == "5")
        return Keys::keyPageUp;
    if (final == '~' && params == "6")
        return Keys::keyPageDown;
    return Keys::nothing;

#endif
}
