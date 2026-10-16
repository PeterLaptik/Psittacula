#ifndef HTTP_CLIENT_INCLUDED_H
#define HTTP_CLIENT_INCLUDED_H

#include <string>

/// Detailed result of an HTTP request
struct HttpResult
{
    long status = 0;     // HTTP status code; 0 = no response (connection error)
    std::string body;    // Response body
    std::string error;   // CURL error, empty on success
    long latency_ms = 0; // Total request duration
};

/// CURL-based http client
class HttpClient
{
    public:
        explicit HttpClient(const std::string &host_and_port, std::string api_key = "");

        HttpClient(const std::string &host, int port, std::string api_key = "");

        HttpClient(const HttpClient &) = delete;

        HttpClient &operator=(const HttpClient &) = delete;

        ~HttpClient();

        void SetApiKey(const std::string &key);

        std::string HttpGet(const std::string &end_point);

        /// GET with HTTP status, error and latency details.
        /// Accepts an end point (concatenated with the host) or a full URL
        HttpResult HttpGetFull(const std::string &end_point, long timeout_ms = 0);

        void HttpPost(const std::string &end_point, const std::string &data, void *receiver = nullptr);

        void HttpPostStream(const std::string &end_point, const std::string &data, void *receiver = nullptr);

    private:
        std::string m_host;
        std::string m_api_key; // optional

        // Owned reference to the process-wide curl global state:
        // set when this object's constructor initialized it successfully
        // (see curl_global_guard.h), released again in the destructor
        bool m_curl_initialized = false;
};

#endif // HTTP_CLIENT_INCLUDED_H
