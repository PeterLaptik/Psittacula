#ifndef CONSOLE_WRITER_INCLUDED_H
#define CONSOLE_WRITER_INCLUDED_H

#include <string>
#ifndef _WIN32
#include <termios.h>
#endif

namespace console {

    enum class TextOrigin {
        normal,
        machine,
        filesystem,
        reasoning,
        splitter,
        error
    };

    class TextReceiver
    {
        public:
            virtual void WriteLine(const std::string &message, TextOrigin origin = TextOrigin::normal) = 0;
            virtual void Write(const std::string &message, TextOrigin origin = TextOrigin::normal) = 0;
            virtual void Clear() = 0;
            virtual void Flush() = 0;
    };

    void set_up_console(TextReceiver *receiver = nullptr);

    TextReceiver* get_current_receiver();

    /// RAII guard: temporarily restores cooked console input so std::cin
    /// line reads (getline, operator>>, get) work inside the raw-mode TUI.
    /// Restores the previous (raw) input mode on destruction.
    class LineInputScope
    {
        public:
            LineInputScope();
            ~LineInputScope();

            LineInputScope(const LineInputScope&) = delete;
            LineInputScope& operator=(const LineInputScope&) = delete;

        private:
#ifdef _WIN32
            void* m_handle = nullptr;
            unsigned long m_mode = 0;
            bool m_saved = false;
#else
            int m_fd = -1;
            struct termios m_termios {};
            bool m_saved = false;
#endif
    };

    void write_line(const std::string &message, TextOrigin origin = TextOrigin::normal);

    void write(const std::string &message, TextOrigin origin = TextOrigin::normal);

    void write_splitter(TextOrigin origin = TextOrigin::splitter);

    void flush();

    void clear();

    inline const char* get_origin_colour(TextOrigin origin);

}

#endif // CONSOLE_WRITER_INCLUDED_H
