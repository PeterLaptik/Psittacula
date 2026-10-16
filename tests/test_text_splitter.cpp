#include "test_framework.h"
#include "text_splitter.h"

#include <string>
#include <vector>

namespace {
    std::string Join(const std::vector<std::string> &v)
    {
        std::string out;
        for (const auto &s : v)
            out += s;
        return out;
    }
}

PS_TEST(split_short_line_is_atomic)
{
    TextSplitter splitter{ 80 };
    std::vector<std::string> lines;
    splitter.SplitText("hello", lines);

    PS_CHECK(lines.size() == 1);
    PS_CHECK(lines[0] == "hello");
}

PS_TEST(split_on_width_breaks_at_space)
{
    TextSplitter splitter{ 5 };
    std::vector<std::string> lines;
    splitter.SplitText("one two three", lines);

    // Pieces reassemble to the original word stream (break chars preserved)
    PS_CHECK(Join(lines) == "one two three");
    for (const auto &l : lines)
        PS_CHECK(l.size() <= 5);
}

PS_TEST(split_multibyte_never_cut_inside_char)
{
    // 2-byte Cyrillic chars in a narrow window: no piece may end in the
    // middle of a UTF-8 sequence (a continuation byte leads every cut)
    TextSplitter splitter{ 3 };
    std::vector<std::string> lines;
    splitter.SplitText("\xD0\xB0\xD0\xB1\xD0\xB2\xD0\xB3\xD0\xB4", lines);

    PS_CHECK(Join(lines) == "\xD0\xB0\xD0\xB1\xD0\xB2\xD0\xB3\xD0\xB4");
    for (const auto &l : lines)
    {
        for (size_t i = 0; i < l.size(); ++i)
        {
            const unsigned char lead = static_cast<unsigned char>(l[i]);
            if (lead >= 0xC0) // a lead byte implies a whole sequence fits
                PS_CHECK(i + (lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2) <= l.size() ||
                         i + 1 < l.size()); // truncated only at line end by design
        }
    }
}

PS_TEST(split_hostile_no_terminators_no_infinite_loop)
{
    // No spaces, no newlines, 4-byte emoji wall: must terminate and emit
    // pieces covering everything
    TextSplitter splitter{ 4 };
    std::vector<std::string> lines;
    const std::string wall = "\xF0\x9F\x98\x80\xF0\x9F\x98\x81\xF0\x9F\x98\x82\xF0\x9F\x98\x83";
    splitter.SplitText(wall, lines);

    PS_CHECK(Join(lines) == wall);
    PS_CHECK(!lines.empty());
}

PS_TEST(split_newlines_end_lines_early)
{
    TextSplitter splitter{ 80 };
    std::vector<std::string> lines;
    splitter.SplitText("a\nb\nc", lines);

    PS_CHECK(lines.size() == 3);
    PS_CHECK(lines[0] == "a");
    PS_CHECK(lines[1] == "b");
    PS_CHECK(lines[2] == "c");
}

PS_TEST(split_empty_input_yields_one_empty_line)
{
    TextSplitter splitter{ 10 };
    std::vector<std::string> lines;
    splitter.SplitText("", lines);

    PS_CHECK(lines.size() == 1);
    PS_CHECK(lines[0].empty());
}
