#ifndef KEYBOARD_INCLUDED_H
#define KEYBOARD_INCLUDED_H

namespace tui {

    // Keyboard input unit
    // Supports UTF-8 characters.
    class Keyboard
    {
        public:
            // Reads key value.
            // On Windows reads UTF-16 via _getwch() (Cyrillic = single unit);
            // on POSIX decodes UTF-8 bytes into Unicode code points.
            // Returns Unicode code point, or one of Keys::* (all negative).
            int ReadKey();

            struct Keys {
                // Special values
                static const int nothing = 0;
                static const int eof = -1;
                static const int keyLeft = -2;
                static const int keyRight = -3;
                static const int keyDel = -4;
                static const int resize = -5;
                static const int keyUp = -6;
                static const int keyDown = -7;
                static const int keyPageUp = -8;
                static const int keyPageDown = -9;
                // Regular values
                static const int keyEnter = 13;
                static const int keyBackSpace = 8;
            };
    };
}

#endif // KEYBOARD_INCLUDED_H