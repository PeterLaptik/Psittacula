#ifndef RESPONSE_READERS_INCLUDED_H
#define RESPONSE_READERS_INCLUDED_H

#include <string>

class ChunkProcessor;

// Response callbacks for CURL

namespace responses_fn {

    size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);

    size_t write_callback_stream(char *ptr, size_t size, size_t nmemb, void *userdata);

    // CURLOPT_XFERINFOFUNCTION: returns non-zero to abort when cancelled
    int progress_abort_on_cancel(void *clientp, long long dltotal, long long dlnow,
        long long ultotal, long long ulnow);

}

#endif // RESPONSE_READERS_INCLUDED_H
