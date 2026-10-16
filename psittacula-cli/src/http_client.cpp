#include "http_client.h"
#include "response_readers.h"
#include "chunk_processor.h"
#include "curl_global_guard.h"
#include "console_writer.h"
#include <chrono>
#include <iostream>
#include <curl/curl.h>

// Console note: this module runs on the worker thread in the TUI build -
// everything user-visible must go through console:: (the TextReceiver),
// a raw std::cout/std::cerr here corrupts the screen layout
HttpClient::HttpClient(const std::string &host_and_port, std::string api_key)
    : m_host(host_and_port), m_api_key(api_key)
{
    m_curl_initialized = curl_global::Acquire();

    if (!m_curl_initialized)
        console::write_line("Curl init error!", console::TextOrigin::error);
}

HttpClient::HttpClient(const std::string &host, int port, std::string api_key)
    : m_host(host + '/' + std::to_string(port)), m_api_key(api_key)
{
    m_curl_initialized = curl_global::Acquire();

    if (!m_curl_initialized)
        console::write_line("Curl init error!", console::TextOrigin::error);
}

HttpClient::~HttpClient()
{
    if (m_curl_initialized)
        curl_global::Release();
}

void HttpClient::SetApiKey(const std::string &key)
{
    m_api_key = key;
}

std::string HttpClient::HttpGet(const std::string &end_point)
{
    CURL *curl = curl_easy_init();
    if (!curl) 
        return "Failed to init curl";

    std::string url = m_host + end_point;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    // Multi-threaded app: never let libcurl install signal handlers / use alarm()
    // for timeouts (e.g. synchronous resolver SIGALRM), it can hit the wrong thread.
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    struct curl_slist *headers = nullptr;
    if (!m_api_key.empty()) 
    {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + m_api_key).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, responses_fn::write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) 
    {
        std::string error = "CURL error: " + std::string(curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return error;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

HttpResult HttpClient::HttpGetFull(const std::string &end_point, long timeout_ms)
{
    HttpResult result;

    CURL *curl = curl_easy_init();
    if (!curl)
    {
        result.error = "Failed to init curl";
        return result;
    }

    bool full_url = end_point.rfind("http://", 0) == 0 || end_point.rfind("https://", 0) == 0;
    std::string url = full_url ? end_point : m_host + end_point;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    // Multi-threaded app: never let libcurl install signal handlers / use alarm()
    // for timeouts (e.g. synchronous resolver SIGALRM), it can hit the wrong thread.
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    if (timeout_ms > 0)
    {
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
    }

    struct curl_slist *headers = nullptr;
    if (!m_api_key.empty())
    {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + m_api_key).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, responses_fn::write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);

    auto start = std::chrono::steady_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto elapsed = std::chrono::steady_clock::now() - start;

    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
    else
        result.error = curl_easy_strerror(res);

    result.latency_ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

void HttpClient::HttpPost(const std::string &end_point, const std::string &data, void *receiver)
{
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        console::write_line("Failed to init curl", console::TextOrigin::error);
        return;
    }

    std::string url = m_host + end_point;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    // Multi-threaded app: never let libcurl install signal handlers / use alarm()
    // for timeouts (e.g. synchronous resolver SIGALRM), it can hit the wrong thread.
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!m_api_key.empty()) 
    {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + m_api_key).c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, responses_fn::write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, receiver);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        console::write_line("CURL error: " + std::string(curl_easy_strerror(res)), console::TextOrigin::error);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

void HttpClient::HttpPostStream(const std::string &end_point, const std::string &data, void *receiver)
{
    CURL *curl = curl_easy_init();

    if (!curl)
    {
        console::write_line("Failed to init curl", console::TextOrigin::error);
        return;
    }

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: text/event-stream");
    if (!m_api_key.empty())
    {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + m_api_key).c_str());
    }
    
    std::string url = m_host + end_point;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    // Multi-threaded app: never let libcurl install signal handlers / use alarm()
    // for timeouts (e.g. synchronous resolver SIGALRM). This handle is used from
    // a worker thread while the main thread blocks in read(); a stray SIGALRM
    // delivered to the wrong thread can interrupt/abort the stream as if
    // CancelRequest() had been called, without the user ever pressing ESC.
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, responses_fn::write_callback_stream);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, receiver);

    // Abort the transfer on UI-thread ESC: poll receiver->IsCancelled()
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, responses_fn::progress_abort_on_cancel);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, receiver);

    // Optional: disable buffering
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

    CURLcode res = curl_easy_perform(curl);

    // Process a possible trailing SSE line that has no final newline
    if (receiver)
        static_cast<ChunkProcessor *>(receiver)->Flush();

    if (res != CURLE_OK && res != CURLE_ABORTED_BY_CALLBACK && res != CURLE_WRITE_ERROR)
        console::write_line("CURL error: " + std::string(curl_easy_strerror(res)), console::TextOrigin::error);


    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}




