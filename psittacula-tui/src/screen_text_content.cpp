#include "screen_text_content.h"
#include "text_splitter.h"
#include <algorithm>

const char *const kDefault = "\033[0m";
const char *const kGreen = "\033[32m";
const char *const kRed = "\033[31m";
const char *const kCyan = "\033[36m";
const char *const kGrey = "\033[90m";

void tui::ScreenTextContent::AddText(const std::string &txt, TextOrigin origin)
{
    if (origin != m_buffer.origin)
    {
        // Flush buffer
        TextLine txt;
        txt.txt = m_buffer.txt;
        txt.origin = m_buffer.origin;
        m_text.push_back(txt);

        m_buffer.txt.clear();
        m_buffer.origin = origin;
    }

    m_buffer.txt += txt;
    RenderText();
}

void tui::ScreenTextContent::SetMaxLineLength(int length)
{
    m_text_line_max_length = length;
    RenderText();
}

int tui::ScreenTextContent::GetMaxLineLength() const
{
    return m_text_line_max_length;
}

int tui::ScreenTextContent::GetRenderedLinesCount() const
{
    return static_cast<int>(m_text_rendered.size());
}

void tui::ScreenTextContent::GetTextWindowForHeight(int height, int shift, std::vector<std::string> &acc) const
{
    if (height <= 0)
        return;

    int max_shift = static_cast<int>(m_text_rendered.size()) - height + 1;
    if (max_shift < 0)
        max_shift = 0;
    if (shift > max_shift)
        shift = max_shift;
    if (shift < 0)
        shift = 0;

    size_t first = acc.size();
    int counter = 1;

    for (int i = static_cast<int>(m_text_rendered.size()) - 1 - shift; counter < height && i >= 0; --i)
    {
        acc.push_back(m_text_rendered[i]);
        counter++;
    }

    std::reverse(acc.begin(), acc.end());
}

void tui::ScreenTextContent::Clear()
{
    m_buffer.txt.clear();
    m_buffer_no_render.txt.clear();
    m_text.clear();
    m_buffer_rendered.clear();
    m_text_rendered.clear();
}

void tui::ScreenTextContent::SplitBuffer()
{
    TextSplitter splitter{ m_text_line_max_length };

    std::vector<std::string> split_lines;
    splitter.SplitText(m_buffer_no_render.txt, split_lines);

    for (int i = 0, arr_size = split_lines.size() - 2; i < arr_size; i++)
    {
        m_text_rendered.push_back(split_lines[i]);
    }

    m_buffer.txt = split_lines[split_lines.size() - 1];
}

void tui::ScreenTextContent::CutByLinesBuffer()
{
    std::vector<std::string> cut_result;
    Split(m_buffer_no_render.txt, cut_result);

    if (cut_result.size() < 2)
        return;

    for (int i = 0, vec_sz = cut_result.size(); i < vec_sz - 1; i++)
    {
        m_text.push_back(TextLine{ cut_result[i], m_buffer_no_render.origin });
        m_text_rendered.push_back(cut_result[i]);
    }

    m_buffer_no_render.txt = cut_result[cut_result.size() - 1];
    m_buffer.txt = m_buffer_no_render.txt;
}


void tui::ScreenTextContent::Split(const std::string &s, std::vector<std::string> &vec)
{
    size_t start = 0;
    while (true)
    {
        size_t pos = s.find('\n', start);
        if (pos == std::string::npos)
        {
            vec.push_back(s.substr(start));
            break;
        }
        vec.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
}

void tui::ScreenTextContent::RenderText()
{
    TextSplitter splitter{ m_text_line_max_length };

    m_text_rendered.clear();

    // Saved text
    std::vector<std::string> split_lines;
    for (auto &line : m_text)
    {
        std::vector<std::string> uncoloured_lines;
        splitter.SplitText(line.txt, uncoloured_lines);

        const char *colour = GetTextColour(line.origin);
        for (auto &r_line : uncoloured_lines)
        {
            split_lines.push_back(std::string(colour) + r_line + std::string(colour));
        }
    }

    const char *buffer_colour = GetTextColour(m_buffer.origin);
    splitter.SplitText(std::string(buffer_colour) + m_buffer.txt + std::string(kDefault), split_lines);

    for (auto &r_line : split_lines)
    {
        m_text_rendered.push_back(r_line);
    }
}

const char* tui::ScreenTextContent::GetTextColour(TextOrigin origin)
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

void tui::ScreenTextContent::PostProcess()
{
}

int tui::ScreenTextContent::utf8_char_len(unsigned char lead)
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

int tui::ScreenTextContent::Utf8Count(const std::string &text)
{
    int count = 0;
    size_t i = 0;
    while (i < text.size())
    {
        size_t step = static_cast<size_t>(utf8_char_len(static_cast<unsigned char>(text[i])));
        if (i + step > text.size())
        {
            ++count;
            break;
        }
        i += step;
        ++count;
    }
    return count;
}
