#ifndef TEXT_SPLITTER_INCLUDED_H
#define TEXT_SPLITTER_INCLUDED_H

#include <string>
#include <vector>

class TextSplitter
{
    /// Spliter to split text content for a set line characters length
    public:
        explicit TextSplitter(int line_size);

        void SplitText(std::string string_to_split, std::vector<std::string>& lines);

        void SetLineSize(int line_size);

    private:
        int m_line_size;
};

#endif //! TEXT_SPLITTER_INCLUDED_H