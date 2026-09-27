#include "llm_connectivity_checker.h"
#include "http_client.h"
#include <algorithm>
#include <chrono>

namespace
{
    const size_t kDetailsMaxChars = 100;

    bool IsSuccess(long http_status)
    {
        return http_status >= 200 && http_status < 300;
    }

    // Makes a single-line truncated snippet from a response body
    std::string MakeDetails(const std::string &body)
    {
        std::string result = body;
        std::replace(result.begin(), result.end(), '\n', ' ');
        std::replace(result.begin(), result.end(), '\r', ' ');
        std::replace(result.begin(), result.end(), '\t', ' ');

        size_t start = result.find_first_not_of(' ');
        if (start == std::string::npos)
            return "";

        size_t end = result.find_last_not_of(' ');
        result = result.substr(start, end - start + 1);

        if (result.size() > kDetailsMaxChars)
            result = result.substr(0, kDetailsMaxChars) + "...";

        return result;
    }
}

LlmConnectivityChecker::LlmConnectivityChecker(const std::string &host_and_port, const std::string &api_key)
    : m_host(host_and_port), m_api_key(api_key)
{ }

void LlmConnectivityChecker::SetApiKey(const std::string &key)
{
    m_api_key = key;
}

void LlmConnectivityChecker::SetTimeoutMs(long timeout_ms)
{
    m_timeout_ms = timeout_ms;
}

LlmConnectivityState LlmConnectivityChecker::Check() const
{
    LlmConnectivityState state;

    // Normalize host: no trailing slashes
    std::string host = m_host;
    while (!host.empty() && host.back() == '/')
        host.pop_back();

    // Both ways: llama.cpp endpoints and OpenAI-compatible endpoints
    struct Target
    {
        std::string name;
        std::string url;
        bool auth;
    };

    std::vector<Target> targets = {
        { "llama.cpp /health",     host + "/health",        false },
        { "llama.cpp /slots",      host + "/slots",         false },
        { "models /v1/models",     host + "/v1/models",     false },
        { "models /models",        host + "/models",        false },
        { "models /api/v1/models", host + "/api/v1/models", false },
    };

    HttpClient http(host, m_api_key);

    bool auth_probe_ok = false;
    bool auth_probe_failed = false;

    for (auto &target : targets)
    {
        LlmEndpointProbe probe = Probe(http, target.name, target.url, target.auth);

        if (probe.http_status > 0)
            state.reachable = true;

        if (probe.ok)
        {
            if (probe.name.rfind("llama.cpp", 0) == 0)
                state.llama_cpp = true;

            if (probe.name.rfind("models", 0) == 0)
                state.openai_compatible = true;

            if (probe.auth)
                auth_probe_ok = true;
        }
        else if (probe.auth && (probe.http_status == 401 || probe.http_status == 403))
        {
            auth_probe_failed = true;
        }

        state.probes.push_back(std::move(probe));
    }

    if (auth_probe_ok)
        state.authorized = true;
    else if (auth_probe_failed)
        state.authorized = false;
    else if (!m_api_key.empty() && state.reachable)
        state.authorized = true;

    // Human-readable summary
    if (!state.reachable)
    {
        state.summary = "No response from " + host + (m_api_key.empty() ? "" : " (API key is set)");
    }
    else
    {
        std::string kind;
        if (state.llama_cpp && state.openai_compatible)
            kind = "llama.cpp (OpenAI-compatible API)";
        else if (state.llama_cpp)
            kind = "llama.cpp";
        else if (state.openai_compatible)
            kind = "OpenAI-compatible";
        else
            kind = "unknown server";

        state.summary = kind + " at " + host;

        if (!m_api_key.empty())
            state.summary += state.authorized ? ", API key accepted" : ", API key rejected";
    }

    return state;
}

LlmEndpointProbe LlmConnectivityChecker::Probe(HttpClient &http, const std::string &name, const std::string &url, bool auth) const
{
    LlmEndpointProbe probe;
    probe.name = name;
    probe.url = url;
    probe.auth = auth;

    HttpResult rsp = http.HttpGetFull(url, m_timeout_ms);

    probe.http_status = rsp.status;
    probe.error = rsp.error;
    probe.latency_ms = rsp.latency_ms;
    probe.ok = IsSuccess(rsp.status);
    probe.details = MakeDetails(rsp.body);

    if (probe.http_status > 0)
        probe.details = "HTTP " + std::to_string(rsp.status) + (probe.details.empty() ? "" : ": " + probe.details);
    else if (!rsp.error.empty())
        probe.details = rsp.error;

    return probe;
}