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
        explicit AiClientImpl(const std::string &host_and_port);

        AiClientImpl(const std::string &host, int port);

        ~AiClientImpl();

        bool CheckHealth() override;

        void SendUserMessage(const std::string &message = "") override;

        void SetReasoning(bool is_shown = true) override;

        void SetApiKey(const std::string &key) override;

        void SetModel(const std::string &model) override;

        void RegisterTool(ToolBase *tool) override;

        virtual void GetToolsInfo(std::vector<std::pair<std::string, std::string>> &tools_acc);

        void SetServerType(AiServerType type) override;

        void ToolUndo() override;

        void ToolRedo() override;

    private:
        void InitTools();
        ToolResponse EvokeTool(ToolCall &call);
        void SendToolsResponses(const std::vector<ToolResponse> &responses);

        DialogueBody m_body_obj;

        Formatter fmt;
        std::string m_api_key;
        HttpClient m_http_client;

        bool m_show_reasoning = true;

        HistoryManager m_history_mgr;
        std::map<std::string, std::unique_ptr<ToolBase>> m_tools_dispatcher;
};

#endif // AI_CLIENT_IMPL_INCLUDED_H
