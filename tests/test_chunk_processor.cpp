#include "test_framework.h"
#include "chunk_processor.h"
#include <vector>

namespace {

    // Collects complete lines fed through the base-class buffering
    class LineCollector : public ChunkProcessor
    {
        public:
            void ProcessChunk(const std::string &chunk) override
            {
                lines.push_back(chunk);
            }

            void GetTokensStat(int &, int &, int &, double &) const override {}

            std::vector<std::string> lines;
    };

} // namespace

PS_TEST(feed_single_complete_lines)
{
    LineCollector c;
    c.Feed("data: {\"a\":1}\n");
    c.Feed("data: {\"b\":2}\n");
    c.Flush();

    PS_CHECK(c.lines.size() == 2);
    PS_CHECK(c.lines[0] == "data: {\"a\":1}");
    PS_CHECK(c.lines[1] == "data: {\"b\":2}");
}

PS_TEST(feed_split_across_reads)
{
    // A JSON object split between two network reads stays buffered until
    // the terminating newline arrives
    LineCollector c;
    c.Feed("data: {\"lon");
    c.Feed("g\": tr");
    PS_CHECK(c.lines.empty()); // nothing complete yet
    c.Feed("ue}\n");
    c.Flush();

    PS_CHECK(c.lines.size() == 1);
    PS_CHECK(c.lines[0] == "data: {\"long\": true}");
}

PS_TEST(feed_trailing_line_without_newline)
{
    LineCollector c;
    c.Feed("data: {\"x\":1}\n");
    c.Feed("data: [DONE]"); // no trailing newline: still delivered by Flush
    c.Flush();

    PS_CHECK(c.lines.size() == 2);
    PS_CHECK(c.lines[1] == "data: [DONE]");
}

PS_TEST(feed_preserves_embedded_empty_lines_and_cr)
{
    // SSE splits events with an empty line; CR must survive inside a line
    LineCollector c;
    c.Feed("data: a\r\n");
    c.Feed("\n");
    c.Feed("data: b\n");
    c.Flush();

    PS_CHECK(c.lines.size() == 3);
    PS_CHECK(c.lines[0] == "data: a\r");
    PS_CHECK(c.lines[1].empty());
    PS_CHECK(c.lines[2] == "data: b");
}

PS_TEST(feed_double_newline_blank_line)
{
    LineCollector c;
    c.Feed("\n\n");
    c.Flush();

    PS_CHECK(c.lines.size() == 2);
    PS_CHECK(c.lines[0].empty());
    PS_CHECK(c.lines[1].empty());
}

PS_TEST(cancel_flag_defaults_false)
{
    LineCollector c;
    PS_CHECK(!c.IsCancelled());
}
