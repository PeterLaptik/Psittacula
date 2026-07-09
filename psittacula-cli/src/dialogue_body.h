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

    private:
        void AddToolCallMessages(const std::vector<ToolResponse> &responses);

        std::string m_model = "any";
        
        // PIMPL for RapidJSON document
        struct RequestJson;
        std::unique_ptr<RequestJson> m_request;
};

#endif // RDIALOGUE_BODY_INCLUDED_H