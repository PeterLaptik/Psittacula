#ifndef AI_CLIENT_INCLUDED_H
#define AI_CLIENT_INCLUDED_H

#include "tool_base.h"
#include <string>
#include <memory>

/// Default interface for the LLM client
///\see AiClientImpl
class AiClient
{
    public:
        AiClient() = default;

        virtual ~AiClient() = default;

        /// Sets the first system message in a dialogue (rules, etc.)
        virtual void SetAgentRules(const std::string &rules) = 0;

        /// Sends message from user
        virtual void SendUserMessage(const std::string &message) = 0;

        /// Sets whether a reasoning text is shown
        virtual void SetReasoning(bool is_shown = true) = 0;

        /// Sets API bearing key, if necessary
        virtual void SetApiKey(const std::string &key) = 0;

        /// Sets model name for all POST data bodies
        virtual void SetModel(const std::string &model) = 0;

        /// Registers tool for all POST data bodies and tool calls
        virtual void RegisterTool(std::unique_ptr<ToolBase> tool) = 0;

        /// Returns tools information: tool name -> tool description pairs
        virtual void GetToolsInfo(std::vector<std::pair<std::string, std::string>> &tools_acc) = 0;

        /// Undo last tool execution
        virtual void ToolUndo() = 0;

        /// Redo last tool execution
        virtual void ToolRedo() = 0;

        /// Returns the first system message from a dialogue of an active session
        virtual std::string GetAgentRules() const = 0;

        /// Returns full data body from the last POST request
        /// May be useful for debug and manua testing
        virtual std::string GetDialogueBody() const = 0;

        /// Clears all context: removes all messages excepting of the first system-role message
        virtual void CleanContext() = 0;
};

#endif // AI_CLIENT_INCLUDED_H