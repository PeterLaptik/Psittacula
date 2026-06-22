#ifndef CHUNK_PROCESOR_INCLUDED_H
#define CHUNK_PROCESOR_INCLUDED_H

#include <string>

/// Interface for POST response chunk processing in a stream mode
class ChunkProcessor
{
    public:
        virtual void ProcessChunk(const std::string &chunk) = 0;
};

#endif // CHUNK_PROCESOR_INCLUDED_H
