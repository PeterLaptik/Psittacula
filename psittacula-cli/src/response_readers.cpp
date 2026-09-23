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

namespace {
    // Passed as CURLOPT_XFERINFODATA: aborts the transfer when cancelled
    int progress_abort_impl(void *clientp)
    {
        auto *processor = static_cast<ChunkProcessor *>(clientp);
        if (processor && processor->IsCancelled())
            return 1; // non-zero aborts curl_easy_perform with CURLE_ABORTED_BY_CALLBACK
        return 0;
    }
}

int responses_fn::progress_abort_on_cancel(void *clientp, long long, long long, long long, long long)
{
    return progress_abort_impl(clientp);
}


size_t responses_fn::write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    static_cast<std::string *>(userp)->append((char *)contents, total);
    return total;
}

size_t responses_fn::write_callback_stream(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    ChunkProcessor *processor = static_cast<ChunkProcessor*>(userdata);
    if (processor && processor->IsCancelled())
        return 0; // abort: signals curl to stop with CURLE_WRITE_ERROR

    size_t total = size * nmemb;
    std::string chunk(ptr, total);

    // Server sends lines like:
    // data: {"id":"...","choices":[{"delta":{"content":"Text"}}]}
    // data: [DONE]

    std::istringstream stream(chunk);
    std::string line;

    while (std::getline(stream, line))
    {
        if(processor)
            processor->ProcessChunk(line);
    }

    if (processor && processor->IsCancelled())
        return 0; // stop as soon as possible after processing pending chunks

    return total;
}


