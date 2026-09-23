#include "text_splitter.h"

#include <string>
#include <vector>

namespace
{
    bool is_break_char(char c)
    {
        return c == ' ' || c == '-' || c == '/' || c == '\\';
    }

    bool is_new_line(char c)
    {
        return c == '\n';
    }

    int utf8_char_len(unsigned char lead)
    {
        if (lead < 0x80)
            return 1;
        if ((lead >> 5) == 0x6)
            return 2;
        if ((lead >> 4) == 0xE)
            return 3;
        if ((lead >> 3) == 0x1E)
            return 4;
        return 1;
    }
}

TextSplitter::TextSplitter(int line_size)
    : m_line_size(line_size)
{ }

void TextSplitter::SplitText(std::string string_to_split, std::vector<std::string>& lines)
{
    if (m_line_size <= 0)
    {
        lines.push_back(string_to_split);
        return;
    }

    size_t start = 0;
    const size_t size = string_to_split.size();

    while (start < size)
    {
        // Only new lines are discarded between lines; spaces and other
        // break characters are content and must be preserved, so they
        // carry over to the start of the next emitted line.
        while (start < size && is_new_line(string_to_split[start]))
            ++start;

        if (start >= size)
            break;

        size_t end = start;
        bool hit_newline = false;
        for (int count = 0; count < m_line_size && end < size; ++count)
        {
            // Force split: a '\n' always ends the current line,
            // regardless of the remaining line width.
            if (is_new_line(string_to_split[end]))
            {
                hit_newline = true;
                break;
            }
            size_t step = static_cast<size_t>(utf8_char_len(static_cast<unsigned char>(string_to_split[end])));
            if (end + step > size)
            {
                end = size;
                break;
            }
            end += step;
        }

        if (hit_newline)
        {
            lines.push_back(string_to_split.substr(start, end - start));
            start = end + 1;
            continue;
        }

        if (end >= size)
        {
            lines.push_back(string_to_split.substr(start));
            break;
        }

        size_t cut = std::string::npos;
        for (size_t i = end; i > start; --i)
        {
            // Break characters are ASCII, so continuation bytes never match.
            if (is_break_char(string_to_split[i - 1]))
            {
                cut = i - 1;
                break;
            }
        }

        if (cut != std::string::npos)
        {
            lines.push_back(string_to_split.substr(start, cut - start + 1));
            start = cut + 1;
        }
        else
        {
            // Safety: an over-long step could leave end == start and loop
            // forever on hostile input; always make progress.
            if (end <= start)
                end = start + 1;
            lines.push_back(string_to_split.substr(start, end - start));
            start = end;
        }
    }

    if (string_to_split.empty())
        lines.push_back(string_to_split);
}

void TextSplitter::SetLineSize(int line_size)
{
    m_line_size = line_size;
}
