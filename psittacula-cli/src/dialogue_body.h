#ifndef RDIALOGUE_BODY_INCLUDED_H
#define RDIALOGUE_BODY_INCLUDED_H

#include "tool_base.h"
#include <string>
#include <map>
#include <memory>

// Full dialogue session: body + data (model, tools with dispatcher, etc)
// The object is sent as a data for completions POST requests
class DialogueBody
{
    public:
         DialogueBody();

         ~DialogueBody();

        void AddUserMessage(const std::string &message);

        /// Removes the last exchange: trailing user message plus any assistant /
        /// tool messages that followed it (used to roll back after ESC cancel)
        void RemoveLastExchange();

        void AddSystemMessage(const std::string &sys_message);

        bool AddResponse(const std::string &response);

        void AddToolResponses(const std::vector<ToolResponse> &responses);

        void ClearHistory();

        void SetModel(const std::string &model);

        void RegisterTool(ToolBase *tool);

        /// Dump full body to a JSON string
        std::string ToJsonString() const;

        /// Dump a dialogue only to a pure text
        std::string ToPureText() const;

        /// Restore body from JSON string
        void FromJsonString(const std::string data);

        std::string GetSystemMessage() const;

        void ClearContext();

        /// Returns a new dialogue body for asking LLM to summarize context
        /// The method implementation returns new dialogue body without msg_num last messages
        std::string GetBodyForSummarizing(int msg_num) const;

        // Compress the dialogue with summarized information
        void Compress(std::string summarized_msg, int msg_left);

    private:
        void AddToolCallMessages(const std::vector<ToolResponse> &responses);
        void PurgePreviousFileContents(const std::vector<ToolResponse> &responses);

        std::string m_model = "any";
        
        // PIMPL for RapidJSON document
        struct RequestJson;
        std::unique_ptr<RequestJson> m_request;
};

#endif // RDIALOGUE_BODY_INCLUDED_H
