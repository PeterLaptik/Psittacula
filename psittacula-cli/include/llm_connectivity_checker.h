#ifndef LLM_CONNECTIVITY_CHECKER_INCLUDED_H
#define LLM_CONNECTIVITY_CHECKER_INCLUDED_H

#include <string>
#include <vector>

class HttpClient;

/// Result of a single endpoint probe
struct LlmEndpointProbe
{
    std::string name;       // Probe description, e.g. "llama.cpp /health"
    std::string url;        // Full probed URL
    bool ok = false;        // HTTP 2xx response received
    bool auth = false;      // Probe requires a valid API key
    long http_status = 0;   // HTTP status code; 0 = no response
    std::string error;      // Transport error, empty on success
    std::string details;    // Short response body snippet
    long latency_ms = 0;    // Request duration
};

/// Aggregated connectivity state of an LLM server
struct LlmConnectivityState
{
    bool reachable = false;         // The host responded over HTTP at least once
    bool llama_cpp = false;         // llama.cpp server detected (/health, /slots)
    bool openai_compatible = false; // OpenAI-compatible API detected (models list)
    bool authorized = false;        // API key accepted, when a key is set
    std::string summary;            // Human-readable summary
    std::vector<LlmEndpointProbe> probes;
};

/// Helper class to check connectivity with an LLM server and its state.
/// Probes both llama.cpp endpoints (/health, /slots) and OpenAI-compatible cloud endpoints via HttpClient / CURL.
class LlmConnectivityChecker
{
    public:
        explicit LlmConnectivityChecker(const std::string &host_and_port, const std::string &api_key = "");

        void SetApiKey(const std::string &key);

        void SetTimeoutMs(long timeout_ms);

        /// Checks connectivity with the host in both ways:
        /// llama.cpp endpoints and OpenAI-compatible endpoints
        LlmConnectivityState Check() const;

    private:
        LlmEndpointProbe Probe(HttpClient &http, const std::string &name, const std::string &url, bool auth) const;

        std::string m_host;
        std::string m_api_key;
        long m_timeout_ms = 5000;
};

#endif // LLM_CONNECTIVITY_CHECKER_INCLUDED_H