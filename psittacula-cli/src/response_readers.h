#ifndef RESPONSE_READERS_INCLUDED_H
#define RESPONSE_READERS_INCLUDED_H

#include <string>

class ChunkProcessor;

namespace responses_fn {

    size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);

    size_t write_callback_stream(char *ptr, size_t size, size_t nmemb, void *userdata);

    //void SetChunkProcessor(ChunkProcessor *proc);

}

#endif // RESPONSE_READERS_INCLUDED_H
