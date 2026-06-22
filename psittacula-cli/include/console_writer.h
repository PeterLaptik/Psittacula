#ifndef CONSOLE_WRITER_INCLUDED_H
#define CONSOLE_WRITER_INCLUDED_H

#include <string>

namespace console {

    enum class TextOrigin {
        default,
        machine,
        filesystem,
        reasoning,
        splitter,
        error
    };

    void set_up_console();

    void write_line(const std::string &message, TextOrigin origin = TextOrigin::default);

    void write(const std::string &message, TextOrigin origin = TextOrigin::default);

    void write_splitter(TextOrigin origin = TextOrigin::splitter);

    void flush();

    void clear();

    void show_history();

    inline const char* get_origin_colour(TextOrigin origin);

}

#endif // CONSOLE_WRITER_INCLUDED_H
