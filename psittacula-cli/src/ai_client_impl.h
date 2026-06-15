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

class AiClientImpl: public AiClient
{
    public:
        explicit AiClientImpl(const std::string &host_and_port, int context_size);

        AiClientImpl(const std::string &host, int port, int context_size);

        ~AiClientImpl() override;

        bool CheckHealth() override;

        void SetAgentRules(const std::string &rules) override;

        void SendUserMessage(const std::string &message = "") override;

        void SetReasoning(bool is_shown = true) override;

        void SetApiKey(const std::string &key) override;

        void SetModel(const std::string &model) override;

        void RegisterTool(std::unique_ptr<ToolBase> tool) override;

        void GetToolsInfo(std::vector<std::pair<std::string, std::string>> &tools_acc) override;

        void SetServerType(AiServerType type) override;

        void ToolUndo() override;

        void ToolRedo() override;

        std::string GetDialogueBody() const override;

        std::string GetAgentRules() const override;

        void CleanContext() override;

    private:
        void InitTools();
        ToolResponse EvokeTool(ToolCall &call);
        void SendToolsResponses(const std::vector<ToolResponse> &tools_responses, const std::string &response = "");
        std::string GetContextInfo();

        DialogueBody m_body_obj;

        Formatter fmt;
        std::string m_api_key;
        int m_context_size = -1;
        HttpClient m_http_client;

        bool m_show_reasoning = true;

        HistoryManager m_history_mgr;
        std::map<std::string, std::unique_ptr<ToolBase>> m_tools_dispatcher;
};

#endif // AI_CLIENT_IMPL_INCLUDED_H