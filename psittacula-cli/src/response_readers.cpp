#include "response_readers.h"
#include "chunk_processor.h"
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>


size_t responses_fn::write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    static_cast<std::string *>(userp)->append((char *)contents, total);
    return total;
}

size_t responses_fn::write_callback_stream(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t total = size * nmemb;
    std::string chunk(ptr, total);

    // Server sends lines like:
    // data: {"id":"...","choices":[{"delta":{"content":"Text"}}]}
    // data: [DONE]

    std::istringstream stream(chunk);
    std::string line;

    ChunkProcessor *processor = static_cast<ChunkProcessor*>(userdata);
    while (std::getline(stream, line))
    {
        if(processor)
            processor->ProcessChunk(line);
    }

    return total;
}


