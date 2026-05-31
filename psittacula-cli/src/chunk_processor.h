#ifndef CHUNK_PROCESOR_INCLUDED_H
#define CHUNK_PROCESOR_INCLUDED_H

#include <string>

class ChunkProcessor
{
    public:
        virtual void ProcessChunk(const std::string &chunk) = 0;
};

#endif // CHUNK_PROCESOR_INCLUDED_H
