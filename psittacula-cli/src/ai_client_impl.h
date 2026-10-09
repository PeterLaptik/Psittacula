#ifndef AI_CLIENT_IMPL_INCLUDED_H
#define AI_CLIENT_IMPL_INCLUDED_H

#include "ai_client.h"
#include "http_client.h"
#include "format_util.h"
#include "history_manager.h"
#include "dialogue_body.h"
#include "json_dialogue.h"
#include "model.h"
#include <atomic>
#include <map>
#include <memory>

class ToolBase;

/// Regular client implementation
///\see AiClient for details
class AiClientImpl: public AiClient
{
    public:
        explicit AiClientImpl(const Model &model);

        ~AiClientImpl() override;

        void SetAgentRules(const std::string &rules) override;

        void SendUserMessage(const std::string &message = "") override;

        void CancelRequest() override;

        void SetApiKey(const std::string &key) override;

        void SetModel(const std::string &model) override;

        void SetLog(bool enabled) override;

        void RegisterTool(std::unique_ptr<ToolBase> tool) override;

        void GetToolsInfo(std::vector<std::pair<std::string, std::string>> &tools_acc) override;

        void ToolUndo() override;

        void ToolRedo() override;

        std::string GetDialogueBody() const override;

        std::string GetDialogueText() const override;

        std::string GetAgentRules() const override;

        void ClearContext() override;

        void CompressContext() override;

        void RestoreDialogueFrom(const std::string &data) override;

    private:
        // Registers all tools in a dispatcher
        void InitTools();

        // Executes a single tool call
        ToolResponse EvokeTool(ToolCall &call);

        // Sends tool responses back to LLM, evokes secondary tools if necessary
        void SendToolsResponses(const std::vector<ToolResponse> &tools_responses, const std::string &response = "");

        // Gets actual context size via /slots endpoint
        std::string GetSlotsInfo();

        // Returns the chat completions endpoint:
        // m_chat_endpoint if set, otherwise chosen by context size (llama.cpp / non-llama)
        std::string GetChatCompletionsEndpoint() const;

        // Autosave through the gate: writes the dialogue body to the log
        // file only when AiClient::m_log is enabled (one check instead of
        // one per call site)
        void WriteLog();

        // POST JSON data object
        // Keeps all messages, tool calls, tool calls responses
        DialogueBody m_body_obj;

        // Autosaves the current dialogue JSON body after each iteration
        // of the exchange with the AI (see SendUserMessage / SendToolsResponses)
        JsonDialogue m_json_dialogue;

        Formatter fmt; // Simple string formatter

        std::string m_api_key;          // Optional: Bearer API key
        int m_context_size = -1;        // Can be set directly, if not set (for llama.cpp) then /slots endpoint is used to get the actual size
        int m_tool_loop_counter = 0;    // Counts tool calls loop iterations to avoid infinite loops
        std::string m_chat_endpoint;

        HttpClient m_http_client; // CURL client for network requests

        HistoryManager m_history_mgr; // Tool calls undo / redo manager
        // Interrupt flag: set by CancelRequest from the UI thread
        std::atomic<bool> m_cancelled{false};
        std::map<std::string, std::unique_ptr<ToolBase>> m_tools_dispatcher; // tool name -> tool prototype object

        // Dialogue autosave flag
        bool m_log = false;
};

#endif // AI_CLIENT_IMPL_INCLUDED_H