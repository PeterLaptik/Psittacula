#ifndef CHUNK_PROCESOR_INCLUDED_H
#define CHUNK_PROCESOR_INCLUDED_H

#include <string>

/// Interface for POST response chunk processing in a stream mode
class ChunkProcessor
{
    public:
        virtual ~ChunkProcessor() = default;

        virtual void ProcessChunk(const std::string &chunk) = 0;

        /// Returns true when the receiver asked to abort the stream (ESC in TUI)
        virtual bool IsCancelled() const { return false; }
};

#endif // CHUNK_PROCESOR_INCLUDED_H
