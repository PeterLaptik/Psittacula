#ifndef AI_CLIENT_INCLUDED_H
#define AI_CLIENT_INCLUDED_H

#include "tool_base.h"
#include <string>
#include <memory>

enum class AiServerType
{
    LlamaCPP,
    OpenRouter
};

class AiClient
{
    public:
        AiClient() = default;

        virtual ~AiClient() = default;

        virtual bool CheckHealth() = 0;

        virtual void SetAgentRules(const std::string &rules) = 0;

        virtual void SendUserMessage(const std::string &message) = 0;

        virtual void SetReasoning(bool is_shown = true) = 0;

        virtual void SetApiKey(const std::string &key) = 0;

        virtual void SetModel(const std::string &model) = 0;

        virtual void RegisterTool(std::unique_ptr<ToolBase> tool) = 0;

        virtual void GetToolsInfo(std::vector<std::pair<std::string, std::string>> &tools_acc) = 0;

        virtual void SetServerType(AiServerType type) = 0;

        virtual void ToolUndo() = 0;

        virtual void ToolRedo() = 0;

        virtual std::string GetAgentRules() const = 0;

        virtual std::string GetDialogueBody() const = 0;

        virtual void CleanContext() = 0;
};

#endif // AI_CLIENT_INCLUDED_H