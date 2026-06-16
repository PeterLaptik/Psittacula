#ifndef AI_CLIENT_IMPL_INCLUDED_H
#define AI_CLIENT_IMPL_INCLUDED_H

#include "ai_client.h"
#include "http_client.h"
#include "format_util.h"
#include "history_manager.h"
#include "dialogue_body.h"
#include <map>
#include <memory>

class ToolBase;

/// Standars client implementation
///\see AiClient for details
class AiClientImpl: public AiClient
{
    public:
        explicit AiClientImpl(const std::string &host_and_port, int context_size);

        AiClientImpl(const std::string &host, int port, int context_size);

        ~AiClientImpl() override;

        void SetAgentRules(const std::string &rules) override;

        void SendUserMessage(const std::string &message = "") override;

        void SetReasoning(bool is_shown = true) override;

        void SetApiKey(const std::string &key) override;

        void SetModel(const std::string &model) override;

        void RegisterTool(std::unique_ptr<ToolBase> tool) override;

        void GetToolsInfo(std::vector<std::pair<std::string, std::string>> &tools_acc) override;

        void ToolUndo() override;

        void ToolRedo() override;

        std::string GetDialogueBody() const override;

        std::string GetAgentRules() const override;

        void CleanContext() override;

    private:
        // Registers all tools in a dispatcher
        void InitTools();

        // Executes a single tool call
        ToolResponse EvokeTool(ToolCall &call);

        // Sends tool responses back to LLM
        void SendToolsResponses(const std::vector<ToolResponse> &tools_responses, const std::string &response = "");

        // Gets actual context size via /slots endpoint
        std::string GetContextInfo();

        // POST JSON data object
        // Keeps all messages, tool calls, tool calls responses
        DialogueBody m_body_obj;

        Formatter fmt; // Simple string formatter

        std::string m_api_key;      // Optional: API bearing key
        int m_context_size = -1;    // Can be set directly, if not set (for llama.cpp) then /slots endpoint is used to get the actual size
        bool m_show_reasoning = true;

        HttpClient m_http_client; // CURL client for network requests

        HistoryManager m_history_mgr; // Tool calls undo / redo manager
        std::map<std::string, std::unique_ptr<ToolBase>> m_tools_dispatcher; // tool name -> tool prototype object
};

#endif // AI_CLIENT_IMPL_INCLUDED_H