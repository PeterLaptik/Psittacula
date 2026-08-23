#include "tool_web_fetch.h"
#include "format_util.h"
#include "console_writer.h"

#include <curl/curl.h>
#include <rapidjson/document.h>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace {

const std::string kWebFetchBase64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string WebFetchBase64Encode(const std::string &data)
{
    std::string out;
    int val = 0, valb = -6;

    for (unsigned char c : data)
    {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0)
        {
            out.push_back(kWebFetchBase64Chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6)
        out.push_back(kWebFetchBase64Chars[((val << 8) >> (valb + 8)) & 0x3F]);

    while (out.size() % 4)
        out.push_back('=');

    return out;
}

struct FetchSink
{
    std::string data;
    size_t max_bytes = 0;
    bool truncated = false;
};

size_t FetchWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    auto *sink = static_cast<FetchSink *>(userp);
    size_t chunk = size * nmemb;

    if (chunk == 0)
        return 0;

    if (sink->max_bytes > 0 && sink->data.size() + chunk > sink->max_bytes)
    {
        size_t remaining = sink->max_bytes - sink->data.size();
        sink->data.append(static_cast<const char *>(contents), remaining);
        sink->truncated = true;
        return 0; // abort the transfer
    }

    sink->data.append(static_cast<const char *>(contents), chunk);
    return chunk;
}

std::string ToLowerCase(const std::string &s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool LooksLikeText(const std::string &content_type)
{
    std::string ct = ToLowerCase(content_type);

    if (ct.empty())
        return true;

    if (ct.find("text/") == 0)
        return true;

    static const char *textual[] = {
        "application/json",
        "application/xml",
        "application/javascript",
        "application/x-www-form-urlencoded",
        "application/svg+xml",
        "application/xhtml+xml",
        "application/csv"
    };

    for (const char *t : textual)
    {
        if (ct.find(t) != std::string::npos)
            return true;
    }

    return false;
}

} // namespace

void WebFetchTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "url",
        "string",
        "URL to fetch. Must start with http:// or https://.",
        true
        });

    params_acc.push_back({
        "method",
        "string",
        "HTTP method to use: GET or POST. Defaults to GET, or POST when body is provided.",
        false
        });

    params_acc.push_back({
        "headers",
        "string",
        "Optional JSON object with extra HTTP headers, e.g. {\"Authorization\":\"Bearer ...\"}.",
        false
        });

    params_acc.push_back({
        "body",
        "string",
        "Request body, used with method=POST.",
        false
        });

    params_acc.push_back({
        "timeout_seconds",
        "integer",
        "Request timeout in seconds.",
        false,
        "30"
        });

    params_acc.push_back({
        "max_length",
        "integer",
        "Maximum number of response bytes to return. Larger responses are truncated.",
        false,
        "100000"
        });

    params_acc.push_back({
        "binary",
        "boolean",
        "If true, return the response body as base64-encoded binary.",
        false
        });

    params_acc.push_back({
        "follow_redirects",
        "boolean",
        "If true, follow HTTP redirects.",
        false,
        "true"
        });

    params_acc.push_back({
        "user_agent",
        "string",
        "User-Agent header value to send.",
        false
        });
}

std::string WebFetchTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    console::write_line("Web fetch tool.", console::TextOrigin::filesystem);

    std::string url = GetParam(params_values, "url");
    if (url.empty())
    {
        console::write_line("Missing required parameter: url", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: url"}})";
    }

    std::string lower_url = ToLowerCase(url);
    if (lower_url.find("http://") != 0 && lower_url.find("https://") != 0)
    {
        console::write_line("Invalid URL: " + url, console::TextOrigin::error);
        std::ostringstream err;
        err << "{\"error\":{\"type\":\"invalid_arguments\",\"message\":\"URL must start with http:// or https://\",\"url\":"
            << GetEscapedJSONString(url) << "}}";
        return err.str();
    }

    std::string method = GetParam(params_values, "method");
    std::string body = GetParam(params_values, "body");

    if (method.empty())
        method = body.empty() ? "GET" : "POST";

    std::string method_lower = ToLowerCase(method);
    if (method_lower != "get" && method_lower != "post")
    {
        std::ostringstream err;
        err << "{\"error\":{\"type\":\"invalid_arguments\",\"message\":\"Unsupported HTTP method\",\"method\":"
            << GetEscapedJSONString(method) << "}}";
        return err.str();
    }

    long timeout_seconds = 30;
    try { timeout_seconds = std::stol(GetParam(params_values, "timeout_seconds", "30")); }
    catch (...) { timeout_seconds = 30; }
    if (timeout_seconds <= 0)
        timeout_seconds = 30;

    size_t max_length = 0;
    try { max_length = static_cast<size_t>(std::stoull(GetParam(params_values, "max_length", "100000"))); }
    catch (...) { max_length = 100000; }

    bool binary_mode = GetParamBool(params_values, "binary", false);
    bool follow_redirects = GetParamBool(params_values, "follow_redirects", true);

    std::string user_agent = GetParam(params_values, "user_agent", "Psittacula-Agent/0.8.3");

    console::write_line(fmt.Format("Fetching: %?", url), console::TextOrigin::filesystem);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    CURL *curl = curl_easy_init();
    if (!curl)
    {
        curl_global_cleanup();
        return R"({"error":{"type":"runtime_error","message":"Failed to initialize libcurl"}})";
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, follow_redirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

    FetchSink sink;
    sink.max_bytes = max_length;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FetchWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, ("User-Agent: " + user_agent).c_str());

    std::string headers_json = GetParam(params_values, "headers");
    if (!headers_json.empty())
    {
        rapidjson::Document doc;
        doc.Parse(headers_json.c_str());

        if (!doc.IsObject())
        {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return R"({"error":{"type":"invalid_arguments","message":"headers must be a JSON object"}})";
        }

        for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it)
        {
            if (!it->value.IsString())
                continue;

            std::string header_line = std::string(it->name.GetString()) + ": " + it->value.GetString();
            headers = curl_slist_append(headers, header_line.c_str());
        }
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (method_lower == "post")
    {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!body.empty())
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }

    CURLcode res = curl_easy_perform(curl);

    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

    char *ct_raw = nullptr;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct_raw);
    std::string content_type = ct_raw ? std::string(ct_raw) : "";

    char *effective_url_raw = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url_raw);
    std::string effective_url = effective_url_raw ? std::string(effective_url_raw) : url;

    bool truncated = sink.truncated;
    bool stopped_by_limit = (res == CURLE_WRITE_ERROR && truncated);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (res != CURLE_OK && !stopped_by_limit)
    {
        std::string error_msg = curl_easy_strerror(res);
        console::write_line(fmt.Format("CURL error: %?", error_msg), console::TextOrigin::error);

        std::ostringstream err;
        err << "{\"error\":{\"type\":\"curl_error\",\"message\":"
            << GetEscapedJSONString(error_msg) << ",\"url\":"
            << GetEscapedJSONString(url) << ",\"status_code\":" << status_code << "}}";
        return err.str();
    }

    bool return_binary = binary_mode || !LooksLikeText(content_type);

    std::ostringstream json;
    json << "{"
        << "\"status\":\"success\","
        << "\"url\":" << GetEscapedJSONString(effective_url) << ","
        << "\"status_code\":" << status_code << ","
        << "\"content_type\":" << GetEscapedJSONString(content_type) << ","
        << "\"content_length\":" << sink.data.size() << ","
        << "\"binary\":" << (return_binary ? "true" : "false") << ","
        << "\"truncated\":" << (truncated ? "true" : "false") << ",";

    if (return_binary)
        json << "\"content_base64\":" << GetEscapedJSONString(WebFetchBase64Encode(sink.data));
    else
        json << "\"content\":" << GetEscapedJSONString(sink.data);

    json << "}";

    return json.str();
}
