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

        std::string ToJsonString() const;

    private:
        std::string m_model = "gpt-4o";

        // Pointer to implementation for RapidJSON document
        struct RequestJson;
        std::unique_ptr<RequestJson> m_request;
};

#endif // RDIALOGUE_BODY_INCLUDED_H
