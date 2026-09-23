#include "input_text_content.h"
#include "text_splitter.h"

namespace
{
    int Utf8CharLenLocal(unsigned char lead)
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

    bool IsContinuation(unsigned char c)
    {
        return (c & 0xC0) == 0x80;
    }
}

void tui::InputTextContent::AddInputLine(const std::string &line)
{
    m_input_lines.push_back(line);

    TextSplitter splitter{ m_text_line_max_length };
    std::vector<std::string> split_lines;
    splitter.SplitText(line, split_lines);

    m_input_lines_rendered.insert(m_input_lines_rendered.end(), split_lines.begin(), split_lines.end());
}

std::string tui::InputTextContent::PopBackInputLine()
{
    std::string last_line;

    if (m_input_lines.empty() || m_input_lines_rendered.empty())
        return std::string();

    std::string last_raw_string = m_input_lines.back();
    std::string last_rendered_string = m_input_lines_rendered.back();
    
    if (last_raw_string == last_rendered_string)
    {
        last_line = m_input_lines.back();
        m_input_lines.pop_back();
        FitInputContentToSize(); // Visible lines quantity has been changed: needs to be updated
    }
    else
    {
        // Hard case: the last raw line was wrapped, so popping must remove
        // only the last rendered piece from the raw remainder — on a
        // CHARACTER boundary. The old byte-based pop_back()/substr() math
        // split 2-byte Cyrillic chars and returned invalid UTF-8.
        last_line = last_rendered_string;
        std::string remainder;
        if (last_rendered_string.size() <= last_raw_string.size() &&
            last_raw_string.compare(last_raw_string.size() - last_rendered_string.size(),
                                    last_rendered_string.size(),
                                    last_rendered_string) == 0)
        {
            remainder = last_raw_string.substr(
                0, last_raw_string.size() - last_rendered_string.size());
        }
        else
        {
            // Rendered view diverged (resize re-wrap): drop the last
            // rendered char count from the raw tail instead of guessing bytes.
            size_t chars_to_drop = 0;
            for (size_t i = 0; i < last_rendered_string.size();)
            {
                size_t step = static_cast<size_t>(
                    Utf8CharLenLocal(static_cast<unsigned char>(last_rendered_string[i])));
                if (i + step > last_rendered_string.size() ||
                    (step > 1 && !IsContinuation(static_cast<unsigned char>(last_rendered_string[i + 1]))))
                    step = 1;
                i += step;
                ++chars_to_drop;
            }
            size_t end = last_raw_string.size();
            while (chars_to_drop > 0 && end > 0)
            {
                do { --end; } while (end > 0 && IsContinuation(
                    static_cast<unsigned char>(last_raw_string[end])));
                --chars_to_drop;
            }
            remainder = last_raw_string.substr(0, end);
        }

        m_input_lines.pop_back();
        if (!remainder.empty())
            m_input_lines.push_back(remainder);
        FitInputContentToSize();
    }
    
    return last_line;
}

void tui::InputTextContent::Clear()
{
    m_input_lines.clear();
    m_input_lines_rendered.clear();
}

void tui::InputTextContent::SetLineLength(int line_length)
{
    m_text_line_max_length = line_length;
    FitInputContentToSize();
}

int tui::InputTextContent::GetLineLength() const
{
    return m_text_line_max_length;
}

bool tui::InputTextContent::Empty() const
{
    return m_input_lines.empty();
}

const std::vector<std::string>&tui::InputTextContent::GetRenderedInputLines() const
{
    return m_input_lines_rendered;
}


void tui::InputTextContent::FitInputContentToSize()
{
    m_input_lines_rendered.clear();

    TextSplitter splitter{m_text_line_max_length};
    std::vector<std::string> split_lines;
    for (const auto &line : m_input_lines)
    {
        splitter.SplitText(line, split_lines);
        m_input_lines_rendered.insert(m_input_lines_rendered.end(), split_lines.begin(), split_lines.end());
        split_lines.clear();
    }
}
