#ifndef CHUNK_PROCESOR_INCLUDED_H
#define CHUNK_PROCESOR_INCLUDED_H

#include <string>

/// Interface for POST response chunk processing in a stream mode
class ChunkProcessor
{
    public:
        virtual ~ChunkProcessor() = default;

        virtual void ProcessChunk(const std::string &chunk) = 0;

        virtual void GetTokensStat(int &total, int &completion, int &prompt, double &cost) const = 0;

        /// Returns true when the receiver asked to abort the stream (ESC in TUI)
        virtual bool IsCancelled() const { return false; }

        /// Feeds a raw portion of received data (a single libcurl read).
        /// An SSE line can be split between two network reads, so the data is
        /// buffered and only complete ('\n'-terminated) lines are passed to
        /// ProcessChunk(). An unfinished tail is kept until the next read.
        /// Call Flush() after the transfer ends to process a possible
        /// trailing line that has no final newline.
        void Feed(const std::string &data)
        {
            m_line_buffer += data;

            size_t start = 0;
            size_t pos = 0;
            while ((pos = m_line_buffer.find('\n', start)) != std::string::npos)
            {
                ProcessChunk(m_line_buffer.substr(start, pos - start));
                start = pos + 1;
            }

            m_line_buffer.erase(0, start);
        }

        /// Processes the buffered tail, if any (call at end of transfer)
        void Flush()
        {
            if (!m_line_buffer.empty())
            {
                ProcessChunk(m_line_buffer);
                m_line_buffer.clear();
            }
        }

    private:
        std::string m_line_buffer; // partial line carried between network reads
};

#endif // CHUNK_PROCESOR_INCLUDED_H
