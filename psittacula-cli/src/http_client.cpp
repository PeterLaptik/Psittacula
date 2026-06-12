#include "http_client.h"
#include "response_readers.h"
#include <iostream>
#include <curl/curl.h>

HttpClient::HttpClient(const std::string &host_and_port, std::string api_key)
    : m_host(host_and_port), m_api_key(api_key)
{
    CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (result != CURLE_OK)
        std::cerr << "Curl init error!" << std::endl;
}

HttpClient::HttpClient(const std::string &host, int port, std::string api_key)
    : m_host(host + '/' + std::to_string(port)), m_api_key(api_key)
{
    CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (result != CURLE_OK)
        std::cerr << "Curl init error!" << std::endl;
}

HttpClient::~HttpClient()
{
    curl_global_cleanup();
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
        return "CURL error: " + std::string(curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

void HttpClient::HttpPost(const std::string &end_point, const std::string &data, void *receiver)
{
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        std::cout << "Failed to init curl" << std::endl;
    }

    std::string url = m_host + end_point;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

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
        std::cout << "CURL error: " + std::string(curl_easy_strerror(res)) << std::endl;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

void HttpClient::HttpPostStream(const std::string &end_point, const std::string &data, void *receiver)
{
    CURL *curl = curl_easy_init();

    if (!curl)
        std::cout << "Failed to init curl" << std::endl;

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

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, responses_fn::write_callback_stream);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, receiver);

    // Optional: disable buffering
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) 
        std::cout << "CURL error: " + std::string(curl_easy_strerror(res)) << std::endl;


    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}




