#ifndef HTTP_CLIENT_INCLUDED_H
#define HTTP_CLIENT_INCLUDED_H

#include <string>

/// CURL-based http client
class HttpClient
{
    public:
        explicit HttpClient(const std::string &host_and_port, std::string api_key = "");

        HttpClient(const std::string &host, int port, std::string api_key = "");

        ~HttpClient();

        void SetApiKey(const std::string &key);

        std::string HttpGet(const std::string &end_point);

        void HttpPost(const std::string &end_point, const std::string &data, void *receiver = nullptr);

        void HttpPostStream(const std::string &end_point, const std::string &data, void *receiver = nullptr);

    private:
        std::string m_host;
        std::string m_api_key; // optional
};

#endif // HTTP_CLIENT_INCLUDED_H
