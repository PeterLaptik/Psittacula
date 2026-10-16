#include "tool_web_fetch.h"
#include "format_util.h"
#include "console_writer.h"
#include "curl_global_guard.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

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

bool IsDecimalsOnly(const std::string &s)
{
    if (s.empty())
        return false;

    for (const char c : s)
    {
        if (c < '0' || c > '9')
            return false;
    }

    return true;
}

/// Host part of a URL, already lowercased, without user info.
/// The URL is expected to be already validated (http:// or https:// prefix).
std::string UrlHostPart(const std::string &lower_url)
{
    const size_t scheme_end = lower_url.find("://");
    if (scheme_end == std::string::npos)
        return "";

    const size_t host_begin = scheme_end + 3;

    // The authority ends at the first '/', '?' or '#'
    size_t host_end = lower_url.size();
    for (size_t i = host_begin; i < lower_url.size(); ++i)
    {
        const char c = lower_url[i];
        if (c == '/' || c == '?' || c == '#')
        {
            host_end = i;
            break;
        }
    }

    std::string host = lower_url.substr(host_begin, host_end - host_begin);

    // Strip userinfo ("user:pass@host"): the check must see the real host
    const size_t at = host.rfind('@');
    if (at != std::string::npos)
        host = host.substr(at + 1);

    return host;
}

/// IPv4 in strict dotted-decimal form (4 groups, 0..255 each).
/// Rejects "127.1" and leading-zero group tricks like "0177.0.0.1".
bool IsIpv4Literal(const std::string &host)
{
    size_t pos = 0;

    for (int group = 0; group < 4; ++group)
    {
        size_t len = 0;
        while (pos + len < host.size() && host[pos + len] >= '0' && host[pos + len] <= '9')
            ++len;

        // Leading zeros are how 0-prefixed octal literals masquerade
        if (len == 0 || len > 3 || (len > 1 && host[pos] == '0'))
            return false;

        int value = 0;
        for (size_t i = 0; i < len; ++i)
            value = value * 10 + (host[pos + i] - '0');

        if (value > 255)
            return false;

        pos += len;

        if (group < 3)
        {
            if (pos >= host.size() || host[pos] != '.')
                return false;
            ++pos;
        }
    }

    return pos == host.size();
}

// First octet value of a strict dotted-quad address, -1 if not one
int Ipv4FirstOctet(const std::string &host)
{
    const size_t dot = host.find('.');
    if (dot == std::string::npos || !IsDecimalsOnly(host.substr(0, dot)) || !IsIpv4Literal(host))
        return -1;

    return std::atoi(host.c_str());
}

// Second octet of a strict dotted-quad address, -1 if not one
int Ipv4SecondOctet(const std::string &host)
{
    const size_t d1 = host.find('.');
    if (d1 == std::string::npos)
        return -1;

    const size_t d2 = host.find('.', d1 + 1);
    if (d2 == std::string::npos)
        return -1;

    const std::string o2 = host.substr(d1 + 1, d2 - d1 - 1);
    return IsDecimalsOnly(o2) ? std::atoi(o2.c_str()) : -1;
}

/// True for loopback / private (RFC 1918) / link-local / default-range IPv4
/// addresses, given the first two octets
bool IsPrivateIpv4Bytes(int o1, int o2)
{
    if (o1 == 127 || o1 == 10 || o1 == 0)
        return true;

    if (o1 == 172 && o2 >= 16 && o2 <= 31)  // 172.16.0.0/12
        return true;

    if (o1 == 192 && o2 == 168)             // 192.168.0.0/16
        return true;

    if (o1 == 169 && o2 == 254)             // 169.254.0.0/16 link-local
        return true;

    return false;
}

bool IsPrivateIpv4String(const std::string &host)
{
    if (!IsIpv4Literal(host))
        return false;

    return IsPrivateIpv4Bytes(std::atoi(host.c_str()), Ipv4SecondOctet(host));
}

/// True when the URL host points at the local machine / private network
/// segment. The URL is expected to be already validated (http:// or https://
/// prefix). Covers hosts the old check missed: ports ("localhost:8080"),
/// userinfo ("user@localhost"), IPv4-mapped and scoped IPv6 forms, the
/// private / link-local / 0.x segments, and localhost-substring hostnames.
bool IsLoopbackUrl(const std::string &url)
{
    const std::string host = UrlHostPart(ToLowerCase(url));

    if (host.empty())
        return true; // no host parsed: treat as suspicious

    if (host == "localhost" || host == "0.0.0.0")
        return true;

    // Bracketed IPv6 forms: [::1], [::], v4-mapped, link-local, ULA
    if (host.size() >= 2 && host.front() == '[' && host.back() == ']')
    {
        const std::string inner = host.substr(1, host.size() - 2);

        if (inner == "::1" || inner == "::")
            return true;

        // v4-mapped / translated forms: ::ffff:a.b.c.d and ::ffff:0:a.b.c.d
        std::string mapped = inner;
        if (mapped.rfind("::ffff:0:", 0) == 0)
            mapped = "::ffff:" + mapped.substr(9);

        if (mapped.rfind("::ffff:", 0) == 0)
        {
            const std::string v4 = mapped.substr(7);
            const int o1 = Ipv4FirstOctet(v4);
            if (o1 < 0)
                return true; // unparsable mapped literal: suspicious
            return IsPrivateIpv4Bytes(o1, Ipv4SecondOctet(v4));
        }

        // Link-local fe80::/10 and unique-local fc00::/7 (fc|fd prefix)
        return inner.rfind("fe80", 0) == 0 ||
               inner.rfind("fc", 0) == 0 || inner.rfind("fd", 0) == 0;
    }

    // Dotted-quad literals
    if (IsPrivateIpv4String(host))
        return true;

    // Hostname fallbacks (also covers the "127.0.0.1.nip.io" family and
    // names like myserver.localhost.internal)
    if (host.find("localhost") != std::string::npos)
        return true;

    return host.rfind("127.", 0) == 0;
}

/// Resolves the URL host and reports whether ANY resolved address lies in a
/// private segment. Only meaningful for hostnames (IP literals were already
/// classified by IsLoopbackUrl, but running them through is harmless and
/// covers the rare exotic-literal forms the string parser rejects).
/// Resolver failure = treated as not private (curl will report its own
/// connection error anyway). Requires an initialized Winsock on Windows -
/// callers keep it behind curl_global::Guard.
bool HostResolvesToPrivateAddress(const std::string &url)
{
    const std::string host = UrlHostPart(ToLowerCase(url));
    if (host.empty() || host.front() == '[')
        return IsLoopbackUrl(url); // bracketed v6: reuse the literal classifier

    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;     // both A and AAAA records
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST; // never recurse into another resolver

    struct addrinfo *result = nullptr;

    // Non-literal hostname: drop the numeric-only flag for the real lookup
    if (IsIpv4Literal(host) || host.rfind("fe80", 0) == 0 ||
        host.find(':') != std::string::npos)
    {
        // literal-shaped: numeric parse only
        if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0)
            return false;
    }
    else
    {
        hints.ai_flags = 0;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0)
            return false; // resolver failure / unknown name: not classified
    }

    bool is_private = false;

    for (struct addrinfo *ai = result; ai != nullptr; ai = ai->ai_next)
    {
        char addr_text[INET6_ADDRSTRLEN] = {};

        if (ai->ai_family == AF_INET)
        {
            auto *sa = reinterpret_cast<struct sockaddr_in *>(ai->ai_addr);
            inet_ntop(AF_INET, &sa->sin_addr, addr_text, sizeof(addr_text));
        }
        else if (ai->ai_family == AF_INET6)
        {
            auto *sa = reinterpret_cast<struct sockaddr_in6 *>(ai->ai_addr);
            inet_ntop(AF_INET6, &sa->sin6_addr, addr_text, sizeof(addr_text));
        }
        else
        {
            continue;
        }

        const std::string resolved = ToLowerCase(addr_text);

        if (resolved.find(':') != std::string::npos)
        {
            // IPv6 result: link-local / ULA / loopback / v4-mapped
            const bool v4mapped = resolved.rfind("::ffff:", 0) == 0;
            if (v4mapped)
            {
                const std::string v4 = resolved.substr(7);
                if (IsPrivateIpv4String(v4))
                    is_private = true;
            }
            else if (resolved.rfind("fe80", 0) == 0 ||
                     resolved.rfind("fc", 0) == 0 || resolved.rfind("fd", 0) == 0 ||
                     resolved == "::1" || resolved == "::")
            {
                is_private = true;
            }
        }
        else if (IsPrivateIpv4String(resolved))
        {
            is_private = true;
        }
    }

    if (result)
        freeaddrinfo(result);

    return is_private;
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
    console::write_line("Web fetch tool.", console::TextOrigin::tools);

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

    std::string user_agent = GetParam(params_values, "user_agent", "Psittacula-Agent/0.9.7");
    std::string headers_json = GetParam(params_values, "headers");

    console::write_line(fmt.Format("Fetching: %?", url), console::TextOrigin::tools);

    // The guard moved above the confirmation gate: the private-target check
    // resolves DNS through getaddrinfo, which needs the Winsock state that
    // curl_global::Acquire initializes on Windows
    curl_global::Guard curl_guard;

    // Anything pointing at the local machine or a private segment goes
    // through confirmation even for a plain GET: an internal admin panel,
    // metadata endpoint or CI runner must not be probed silently.
    // Literal forms are classified by strings; hostnames by DNS resolution.
    const bool private_target = IsLoopbackUrl(url) || HostResolvesToPrivateAddress(url);

    // Notifications and POST/PUT submissions can leak data or trigger
    // side effects on a remote service, so they go through the same
    // confirmation gate as run_command. Plain GET queries to public
    // hosts stay silent.
    if (method_lower == "post" || !body.empty() || !headers_json.empty() ||
        private_target)
    {
        std::string summary = method_lower + " " + url;

        if (private_target && IsLoopbackUrl(url))
            summary += " [LOCAL/PRIVATE ADDRESS]";

        if (private_target && !IsLoopbackUrl(url))
            summary += " [resolves to a private address]";

        if (!body.empty())
            summary += fmt.Format(" (body: %? bytes)", body.size());

        if (!headers_json.empty())
            summary += fmt.Format(" (headers: %?)", headers_json);

        std::string confirm_text = fmt.Format(
            "web_fetch %?", summary);

        if (!console::ask_confirm(confirm_text))
        {
            console::write_line("Operation cancelled by user.", console::TextOrigin::tools);
            return R"({"error":{"type":"operation_cancelled","message":"Operation cancelled by user"}})";
        }
    }

    CURL *curl = curl_easy_init();
    if (!curl)
    {
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

    if (!headers_json.empty())
    {
        rapidjson::Document doc;
        doc.Parse(headers_json.c_str());

        if (!doc.IsObject())
        {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
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
