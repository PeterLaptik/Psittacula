#include "dialogue_body.h"
#include <iostream>
#include <algorithm>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/error/en.h>

// Double escaping backslashes
auto prepare_obj_string = [](const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
};

struct DialogueBody::RequestJson
{
    rapidjson::Document body;
};

DialogueBody::DialogueBody()
    : m_request(std::make_unique<RequestJson>())
{
    m_request->body.SetObject();
    rapidjson::Document::AllocatorType &alloc = m_request->body.GetAllocator();

    m_request->body.AddMember("stream", true, alloc);
    m_request->body.AddMember("reasoning_format", "auto", alloc);
    m_request->body.AddMember("return_progress", true, alloc);
    m_request->body.AddMember("timings_per_token", true, alloc);

    m_request->body.AddMember("model", rapidjson::Value(m_model.c_str(), alloc).Move(), alloc);

    rapidjson::Value messages(rapidjson::kArrayType);
    m_request->body.AddMember("messages", messages, alloc);

    rapidjson::Value tools(rapidjson::kArrayType);
    m_request->body.AddMember("tools", tools, alloc);
}

DialogueBody::~DialogueBody()
{ }


void DialogueBody::AddUserMessage(const std::string &message)
{
    auto it = m_request->body.FindMember("messages");
    if (it != m_request->body.MemberEnd() && it->value.IsArray()) 
    {
        rapidjson::Document::AllocatorType &alloc = m_request->body.GetAllocator();
        rapidjson::Value msg_value(rapidjson::kObjectType);
        msg_value.AddMember("role", "user", alloc);
        msg_value.AddMember("content", rapidjson::Value(message.c_str(), alloc).Move(), alloc);
        it->value.PushBack(msg_value, alloc);
    }
}

void DialogueBody::AddSystemMessage(const std::string &sys_message)
{
    auto it = m_request->body.FindMember("messages");
    if (it != m_request->body.MemberEnd() && it->value.IsArray())
    {
        rapidjson::Value &messages = it->value;
        rapidjson::Document::AllocatorType &alloc = m_request->body.GetAllocator();

        for (rapidjson::SizeType i = 0; i < messages.Size(); )
        {
            const rapidjson::Value &msg = messages[i];
            if (msg.HasMember("role") && msg["role"].IsString() &&
                std::string(msg["role"].GetString()) == "system")
            {
                messages.Erase(messages.Begin() + i);
                continue; // do NOT increment i
            }
            ++i;
        }

        rapidjson::Value msg_value(rapidjson::kObjectType);
        msg_value.AddMember("role", rapidjson::Value("system", alloc), alloc);
        msg_value.AddMember("content", rapidjson::Value(sys_message.c_str(), alloc), alloc);

        // Insert at index 0 manually
        messages.PushBack(rapidjson::Value(), alloc);
        for (rapidjson::SizeType i = messages.Size() - 1; i > 0; --i)
        {
            messages[i] = messages[i - 1];
        }
        messages[0] = msg_value; // place system message at front
    }
}

bool DialogueBody::AddResponse(const std::string &response)
{
    auto is_all_whitespace = [](const std::string &s) {
        return std::all_of(s.begin(), s.end(),
            [](unsigned char c) { return std::isspace(c); });
        };

    if (response.empty() || is_all_whitespace(response))
        return false;

    if (response.empty())
        return false;

    auto it = m_request->body.FindMember("messages");
    if (it != m_request->body.MemberEnd() && it->value.IsArray())
    {
        rapidjson::Document::AllocatorType &alloc = m_request->body.GetAllocator();
        rapidjson::Value msg_value(rapidjson::kObjectType);
        msg_value.AddMember("role", "assistant", alloc);
        msg_value.AddMember("content", rapidjson::Value(response.c_str(), alloc).Move(), alloc);
        it->value.PushBack(msg_value, alloc);
    }

    return true;
}

void DialogueBody::AddToolResponses(const std::vector<ToolResponse> &responses)
{
    AddToolCallMessages(responses);

    auto it = m_request->body.FindMember("messages");
    if (it != m_request->body.MemberEnd() && it->value.IsArray())
    {
        rapidjson::Document::AllocatorType &alloc = m_request->body.GetAllocator();

        for (const ToolResponse &rss : responses)
        {
            // Tool result message
            rapidjson::Document::AllocatorType &assist_alloc = m_request->body.GetAllocator();
            rapidjson::Value tool_msg(rapidjson::kObjectType);
            tool_msg.AddMember("tool_call_id", rapidjson::Value(rss.id.c_str(), assist_alloc).Move(), assist_alloc);
            tool_msg.AddMember("role", "tool", alloc);
            tool_msg.AddMember("recipient", rapidjson::Value(rss.name.c_str(), assist_alloc).Move(), assist_alloc);

            rapidjson::Value content_val;
            content_val.SetString(rss.output_content.c_str(), alloc);

            tool_msg.AddMember("content", rapidjson::Value(rss.output_content.c_str(), assist_alloc).Move(), alloc);
            it->value.PushBack(tool_msg, alloc);
        }
    }
}

void DialogueBody::ClearHistory()
{
    auto it = m_request->body.FindMember("messages");
    if (it != m_request->body.MemberEnd() && it->value.IsArray())
    {
        it->value.Clear();  // remove all previous messages
    }
}

void DialogueBody::SetModel(const std::string &model)
{
    std::cout << "Setting up model: " << model << std::endl;
    if (!m_request->body.HasMember("model"))
    {
        std::cout << "\033[31mInternal error: no body field for model!" << std::endl;
        return;
    }
    m_request->body["model"].SetString(model.c_str(), m_request->body.GetAllocator());
}

void DialogueBody::RegisterTool(ToolBase *tool)
{
    rapidjson::Document::AllocatorType &alloc = m_request->body.GetAllocator();

    auto it = m_request->body.FindMember("tools");
    if (it == m_request->body.MemberEnd())
    {
        std::cout << "tools_error" << std::endl;
        return;
    }

    rapidjson::Value tool_info(rapidjson::kObjectType);
    tool_info.AddMember("type", "function", alloc);

    rapidjson::Value tool_function(rapidjson::kObjectType);
    tool_function.AddMember("name",
        rapidjson::Value(tool->GetToolName().c_str(), alloc).Move(), alloc);

    tool_function.AddMember("description",
        rapidjson::Value(tool->GetToolDescription().c_str(), alloc).Move(), alloc);

    // parameters object
    rapidjson::Value parameters(rapidjson::kObjectType);
    parameters.AddMember("type", "object", alloc);

    // properties
    rapidjson::Value properties(rapidjson::kObjectType);

    std::vector<ToolParameter> props;
    tool->GetParameters(props);

    rapidjson::Value required(rapidjson::kArrayType);

    for (auto &prop : props)
    {
        rapidjson::Value prop_schema(rapidjson::kObjectType);

        prop_schema.AddMember("type",
            rapidjson::Value(prop.type.c_str(), alloc).Move(), alloc);

        prop_schema.AddMember("description",
            rapidjson::Value(prop.description.c_str(), alloc).Move(), alloc);

        if (!prop.default.empty())
        {
            if (prop.type == "string")
            {
                prop_schema.AddMember("default",
                    rapidjson::Value(prop.default.c_str(), alloc).Move(), alloc);
            }
            else if (prop.type == "integer")
            {
                int value = 0;
                try
                {
                    value = std::atoi(prop.default.c_str());
                    prop_schema.AddMember("default", rapidjson::Value(value), alloc);
                }
                catch (...) { }
            }
            else if (prop.type == "boolean")
            {
                bool value = prop.type == "true" ? true : false;
                prop_schema.AddMember("default", rapidjson::Value(value), alloc);
            }
            else if (prop.type == "number")
            {
                double value = 0;
                try
                {
                    value = std::atof(prop.default.c_str());
                    prop_schema.AddMember("default", rapidjson::Value(value), alloc);
                }
                catch (...) {}
            }
            else
            {
                std::cerr << "Tool registring: bad parameter type for " << prop.name << std::endl;
            }
        }

        properties.AddMember(
            rapidjson::Value(prop.name.c_str(), alloc).Move(),
            prop_schema,
            alloc
        );

        if (prop.is_required)
        {
            required.PushBack(
                rapidjson::Value(prop.name.c_str(), alloc).Move(),
                alloc
            );
        }
    }

    parameters.AddMember("properties", properties, alloc);
    parameters.AddMember("required", required, alloc);

    tool_function.AddMember("parameters", parameters, alloc);

    tool_info.AddMember("function", tool_function, alloc);

    it->value.PushBack(tool_info, alloc);
}

std::string DialogueBody::ToJsonString() const
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    m_request->body.Accept(writer);
    return buffer.GetString();
}

std::string DialogueBody::GetSystemMessage() const
{
    auto it = m_request->body.FindMember("messages");
    if (it != m_request->body.MemberEnd() && it->value.IsArray())
    {
        const rapidjson::Value &messages = it->value;
        for (rapidjson::SizeType i = 0; i < messages.Size(); ++i)
        {
            const rapidjson::Value &msg = messages[i];
            if (msg.HasMember("role") && msg["role"].IsString() &&
                std::string(msg["role"].GetString()) == "system")
            {
                if (msg.HasMember("content") && msg["content"].IsString())
                {
                    return std::string(msg["content"].GetString());
                }
                return "";
            }
        }
    }
    return "";
}

void DialogueBody::AddToolCallMessages(const std::vector<ToolResponse> &responses)
{
    auto it = m_request->body.FindMember("messages");
    if (it != m_request->body.MemberEnd() && it->value.IsArray())
    {
        rapidjson::Document::AllocatorType &assist_alloc = m_request->body.GetAllocator();
        rapidjson::Value assist_msg_value(rapidjson::kObjectType);
        assist_msg_value.AddMember("role", "assistant", assist_alloc);
        assist_msg_value.AddMember("content", rapidjson::Value(rapidjson::kNullType), assist_alloc);

        rapidjson::Value arr(rapidjson::kArrayType);
        for (const ToolResponse &rsp : responses)
        {
            rapidjson::Value obj(rapidjson::kObjectType);
            obj.AddMember("id", rapidjson::Value(rsp.id.c_str(), assist_alloc).Move(), assist_alloc);
            obj.AddMember("type", rapidjson::Value("function", assist_alloc), assist_alloc);

            rapidjson::Value inner_obj(rapidjson::kObjectType);
            inner_obj.AddMember("name", rapidjson::Value(rsp.name.c_str(), assist_alloc).Move(), assist_alloc);
            inner_obj.AddMember("arguments", rapidjson::Value(rsp.input_content.c_str(), assist_alloc).Move(), assist_alloc);

            obj.AddMember("function", inner_obj, assist_alloc);

            arr.PushBack(obj, assist_alloc);
        }

        assist_msg_value.AddMember("tool_calls", arr, assist_alloc);

        it->value.PushBack(assist_msg_value, assist_alloc);
    }
}