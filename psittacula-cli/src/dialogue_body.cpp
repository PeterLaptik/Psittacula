#include "dialogue_body.h"
#include "console_writer.h"
#include "utf8_util.h"
#include <iostream>
#include <algorithm>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/error/en.h>

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
        const std::string safe_message = utf8::Sanitize(message);
        msg_value.AddMember("content", rapidjson::Value(safe_message.c_str(), static_cast<rapidjson::SizeType>(safe_message.size()), alloc).Move(), alloc);
        it->value.PushBack(msg_value, alloc);
    }
}

void DialogueBody::RemoveLastExchange()
{
    auto it = m_request->body.FindMember("messages");
    if (it == m_request->body.MemberEnd() || !it->value.IsArray())
        return;

    rapidjson::Value &messages = it->value;
    if (messages.Empty())
        return;

    // Find the last user message; erase it and everything after it
    // (partial assistant reply, tool calls, tool results)
    rapidjson::SizeType last_user = messages.Size();
    for (rapidjson::SizeType i = messages.Size(); i > 0; --i)
    {
        const rapidjson::Value &msg = messages[i - 1];
        if (msg.HasMember("role") && msg["role"].IsString() &&
            std::string(msg["role"].GetString()) == "user")
        {
            last_user = i - 1;
            break;
        }
    }

    if (last_user < messages.Size())
        messages.Erase(messages.Begin() + last_user, messages.End());
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
        const std::string safe_sys_message = utf8::Sanitize(sys_message);
        msg_value.AddMember("content", rapidjson::Value(safe_sys_message.c_str(), static_cast<rapidjson::SizeType>(safe_sys_message.size()), alloc), alloc);

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

    auto it = m_request->body.FindMember("messages");
    if (it != m_request->body.MemberEnd() && it->value.IsArray())
    {
        rapidjson::Document::AllocatorType &alloc = m_request->body.GetAllocator();
        rapidjson::Value msg_value(rapidjson::kObjectType);
        msg_value.AddMember("role", "assistant", alloc);

        const std::string safe_response = utf8::Sanitize(response);
        msg_value.AddMember("content", rapidjson::Value(safe_response.c_str(), static_cast<rapidjson::SizeType>(safe_response.size()), alloc).Move(), alloc);
        it->value.PushBack(msg_value, alloc);
    }

    return true;
}

void DialogueBody::AddToolResponses(const std::vector<ToolResponse> &responses)
{
    AddToolCallMessages(responses);

    PurgePreviousFileContents(responses);

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

            // Tool output can contain arbitrary bytes (console output, file contents):
            // invalid UTF-8 makes the server reject the whole request body
            const std::string safe_content = utf8::Sanitize(rss.output_content);
            tool_msg.AddMember("content", rapidjson::Value(safe_content.c_str(), static_cast<rapidjson::SizeType>(safe_content.size()), alloc).Move(), alloc);
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
    console::write("Setting up model: " + model + " \n --- \n ", console::TextOrigin::tools);
    if (!m_request->body.HasMember("model"))
    {
        console::write_line("Internal error: no body field for model!", console::TextOrigin::error);
        return;
    }
    m_request->body["model"].SetString(model.c_str(), m_request->body.GetAllocator());

    m_model = model;
}

void DialogueBody::RegisterTool(ToolBase *tool)
{
    rapidjson::Document::AllocatorType &alloc = m_request->body.GetAllocator();

    auto it = m_request->body.FindMember("tools");
    if (it == m_request->body.MemberEnd())
    {
        console::write_line("Register tool error: no 'tools' field in the request body", console::TextOrigin::error);
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

        if (!prop.default_value.empty())
        {
            if (prop.type == "string")
            {
                prop_schema.AddMember("default",
                    rapidjson::Value(prop.default_value.c_str(), alloc).Move(), alloc);
            }
            else if (prop.type == "integer")
            {
                int value = 0;
                try
                {
                    value = std::atoi(prop.default_value.c_str());
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
                    value = std::atof(prop.default_value.c_str());
                    prop_schema.AddMember("default", rapidjson::Value(value), alloc);
                }
                catch (...) {}
            }
            else
            {
                console::write_line("Tool registering: bad parameter type for " + prop.name, console::TextOrigin::error);
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

    // Final gate: a single ill-formed UTF-8 byte makes the server reject the whole
    // request with a JSON parse error, so never emit an invalid sequence
    return utf8::Sanitize(std::string(buffer.GetString(), buffer.GetSize()));
}

std::string DialogueBody::ToPureText() const
{
    std::string pure_text;
    auto it = m_request->body.FindMember("messages");
    if (it == m_request->body.MemberEnd() || !it->value.IsArray())
        return pure_text;

    for (auto &msg : it->value.GetArray()) {
        if (!msg.IsObject() || !msg.HasMember("role") || !msg.HasMember("content"))
            continue;

        if (!msg["role"].IsString())
            continue;

        if (msg["content"].IsNull())
            continue;

        if (!msg["content"].IsString())
            continue;

        std::string role = msg["role"].GetString();
        std::string message = role != "tool" ? msg["content"].GetString() : "...";
        pure_text += "[" + role + "]: \n" + message + "\n";
        pure_text += "--------------------------\n";
    }
    return pure_text;
}

void DialogueBody::FromJsonString(const std::string &data)
{
    // Clears the document and parses the new JSON
    m_request->body.Parse(data.c_str());
    
    // Check for parse errors
    if (m_request->body.HasParseError())
    {
        console::write_line(std::string("JSON parse error: ") + rapidjson::GetParseError_En(m_request->body.GetParseError()), console::TextOrigin::error);
        return;
    }

    // Restoring is only possible from a dialogue body: an arbitrary JSON
    // (e.g. a server error object) without a 'messages' array is rejected
    auto msg_it = m_request->body.FindMember("messages");
    if (msg_it == m_request->body.MemberEnd() || !msg_it->value.IsArray())
    {
        console::write_line("JSON restore error: no 'messages' array", console::TextOrigin::error);
        return;
    }

    // Keep the cached model name in sync with the restored body
    auto model_it = m_request->body.FindMember("model");
    if (model_it != m_request->body.MemberEnd() && model_it->value.IsString())
        m_model = model_it->value.GetString();

    const rapidjson::Value &messages = msg_it->value;
    for (auto &msg : messages.GetArray()) {
        if (!msg.IsObject() || !msg.HasMember("role") || !msg["role"].IsString())
            continue;

        if (msg.HasMember("content") && !msg["content"].IsString() && !msg["content"].IsNull())
            continue;

        std::string role = msg["role"].GetString();
        if (role == "tool")
            continue;

        std::string message = "[Null content]";
        if (msg.HasMember("content") && !msg["content"].IsNull())
        {
            message = msg["content"].GetString();
        }
        else // On tool calls
        {
            if (msg.HasMember("tool_calls") && msg["tool_calls"].IsArray() && !msg["tool_calls"].Empty())
            {
                const auto &toolCalls = msg["tool_calls"];
                const auto &call = toolCalls[0]; // usually one per message

                if (call.IsObject() && call.HasMember("function") && call["function"].IsObject() &&
                    call["function"].HasMember("name") && call["function"]["name"].IsString())
                {
                    std::string toolName = call["function"]["name"].GetString();

                    role = "tool";
                    message = "Tool call: " + toolName;
                }
            }
        }

        console::TextOrigin origin = console::TextOrigin::normal;
        if (role == "system")
            origin = console::TextOrigin::tools;
        else if (role == "assistant")
            origin = console::TextOrigin::machine;
        else if(role == "tool")
            origin = console::TextOrigin::tools;

        if (role != "system")
        {
            console::write_line(message, origin);
            console::write_splitter();
        }
    }
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

void DialogueBody::ClearContext()
{
    auto it = m_request->body.FindMember("messages");
    if (it != m_request->body.MemberEnd() && it->value.IsArray())
    {
        rapidjson::Value &messages = it->value;
        rapidjson::Document::AllocatorType &alloc = m_request->body.GetAllocator();

        // Find the first system message
        rapidjson::Value system_msg;
        bool has_system = false;

        for (rapidjson::SizeType i = 0; i < messages.Size(); ++i)
        {
            const rapidjson::Value &msg = messages[i];
            if (msg.HasMember("role") && msg["role"].IsString() &&
                std::string(msg["role"].GetString()) == "system")
            {
                system_msg.CopyFrom(msg, alloc);
                has_system = true;
                break;
            }
        }

        // Clear all messages
        messages.Clear();

        // Restore the first system message if it existed
        if (has_system)
        {
            messages.PushBack(system_msg, alloc);
        }
    }
}

// Exchange previous file contents with a success message if the file was read in the current responses
void DialogueBody::PurgePreviousFileContents(const std::vector<ToolResponse> &responses)
{
    if (responses.empty())
        return;

    auto it = m_request->body.FindMember("messages");
    if (it == m_request->body.MemberEnd() || !it->value.IsArray())
        return;

    rapidjson::Value &messages = it->value;
    rapidjson::SizeType messages_size = messages.Size();

    // Collect file paths from the current read_file call arguments
    // to purge obsolete file contents later
    std::vector<std::string> current_file_paths;
    for (const ToolResponse &rsp : responses)
    {
        if (rsp.name != "read_file" || rsp.input_content.empty())
            continue;

        rapidjson::Document parsed;
        parsed.Parse(rsp.input_content.c_str());
        if (parsed.HasParseError() || !parsed.IsObject())
            continue;

        if (parsed.HasMember("path") && parsed["path"].IsString())
            current_file_paths.emplace_back(parsed["path"].GetString());
    }

    if (current_file_paths.empty())
        return;

    // Iterate through all previous tool messages (role == "tool")
    for (rapidjson::SizeType idx = 0; idx < messages_size; idx++)
    {
        rapidjson::Value &msg = messages[idx];

        // Skip non-tool messages
        if (!msg.IsObject() || !msg.HasMember("role") || !msg["role"].IsString() ||
            std::string(msg["role"].GetString()) != "tool")
        {
            continue;
        }

        // Only process messages with "recipient" == 'read_file'
        if (!msg.HasMember("recipient") || !msg["recipient"].IsString() ||
            std::string(msg["recipient"].GetString()) != "read_file")
        {
            continue;
        }

        if (!msg.HasMember("content") || !msg["content"].IsString())
            continue;

        rapidjson::Document parsed;
        parsed.Parse(msg["content"].GetString());
        if (parsed.HasParseError() || !parsed.IsObject())
            continue;

        if (!parsed.HasMember("file") || !parsed["file"].IsObject())
            continue;

        const rapidjson::Value &file = parsed["file"];

        // A binary read has 'content_base64' instead of 'content': nothing to purge
        if (!file.HasMember("path") || !file["path"].IsString() ||
            !file.HasMember("content") || !file["content"].IsString())
        {
            continue;
        }

        const char *prev_file_path = file["path"].GetString();

        bool is_current = false;
        for (const auto &current_path : current_file_paths)
        {
            if (prev_file_path == current_path)
            {
                is_current = true;
                break;
            }
        }

        if (!is_current)
            continue;

        // Replace the obsolete file content with a short note
        parsed["file"]["content"].SetString(
            "File was successfully read. See updated content in the latest response.",
            parsed.GetAllocator());

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        parsed.Accept(writer);

        // Replace tool message content
        rapidjson::Document::AllocatorType &alloc = m_request->body.GetAllocator();
        msg["content"].SetString(buffer.GetString(), alloc);
    }
}

std::string DialogueBody::GetBodyForSummarizing(int msg_num) const
{
    auto it = m_request->body.FindMember("messages");
    if (it == m_request->body.MemberEnd() || !it->value.IsArray())
    {
        return "";
    }
    
    const rapidjson::Value &messages = it->value;
    
    if (msg_num <= 0 || static_cast<int>(messages.Size()) <= msg_num + 1)
    {
        return ToJsonString();
    }
    
    // Create a new document for the summarization request
    rapidjson::Document summary_body;
    summary_body.SetObject();
    rapidjson::Document::AllocatorType &alloc = summary_body.GetAllocator();
    
    std::string model_name = m_model;
    auto model_it = m_request->body.FindMember("model");
    if (model_it != m_request->body.MemberEnd() && model_it->value.IsString())
        model_name = model_it->value.GetString();

    summary_body.AddMember("model",
        rapidjson::Value(model_name.c_str(), static_cast<rapidjson::SizeType>(model_name.size()), alloc).Move(),
        alloc);
    
    // Copy all messages except the last msg_num messages
    rapidjson::Value summary_messages(rapidjson::kArrayType);
    for (rapidjson::SizeType i = 0; i < messages.Size(); ++i)
    {
        if (static_cast<int>(i) >= static_cast<int>(messages.Size()) - msg_num)
        {
            continue; // Skip last msg_num messages
        }
        rapidjson::Value copy_val;
        copy_val.CopyFrom(messages[i], alloc);
        summary_messages.PushBack(copy_val, alloc);
    }

    // Add a user message asking the LLM to summarize the dialogue
    rapidjson::Value summary_msg(rapidjson::kObjectType);
    summary_msg.AddMember("role", rapidjson::Value("user", alloc), alloc);
    summary_msg.AddMember("content", rapidjson::Value(
        "Please summarize the above dialogue to compress the context. "
        "Provide a concise summary that captures the essential conversation flow, "
        "key decisions, and important information. The summary should be suitable "
        "for replacing the full dialogue in future interactions.", alloc).Move(), alloc);
    summary_messages.PushBack(summary_msg, alloc);

    summary_body.AddMember("messages", summary_messages, alloc);
    
    // Copy tools (for context during summarization)
    rapidjson::Value tools(rapidjson::kArrayType);
    summary_body.AddMember("tools", tools, alloc);
    
    // Add other request fields
    summary_body.AddMember("stream", true, alloc);
    summary_body.AddMember("reasoning_format", "auto", alloc);
    summary_body.AddMember("return_progress", true, alloc);
    summary_body.AddMember("timings_per_token", true, alloc);
    
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    summary_body.Accept(writer);
    
    return utf8::Sanitize(std::string(buffer.GetString(), buffer.GetSize()));
}

void DialogueBody::Compress(std::string summarized_msg, int msg_left)
{
    auto it = m_request->body.FindMember("messages");
    if (it == m_request->body.MemberEnd() || !it->value.IsArray())
    {
        return;
    }
    
    rapidjson::Value &messages = it->value;
    rapidjson::Document::AllocatorType &alloc = m_request->body.GetAllocator();
    
    rapidjson::SizeType total_messages = messages.Size();
    
    if (total_messages <= 1)
    {
        return;
    }
    
    // Create a new array with the system message, summarized message, and last msg_left messages
    rapidjson::Value new_messages(rapidjson::kArrayType);
    
    // Keep the first message (system message)
    rapidjson::Value first_msg;
    first_msg.CopyFrom(messages[0], alloc);
    new_messages.PushBack(first_msg, alloc);
    

    const std::string safe_summary = utf8::Sanitize(summarized_msg);
    rapidjson::Value summary_msg(rapidjson::kObjectType);
    summary_msg.AddMember("role", rapidjson::Value("assistant", alloc), alloc);
    summary_msg.AddMember("content", rapidjson::Value(safe_summary.c_str(), static_cast<rapidjson::SizeType>(safe_summary.size()), alloc).Move(), alloc);
    new_messages.PushBack(summary_msg, alloc);
    
    // Add the last msg_left messages.
    // A kept window must not start with a 'tool' message its parent assistant
    // 'tool_calls' message would be dropped, and a tool result without a
    // preceding tool_calls message is rejected by strict OpenAI-compatible
    // servers. Skip such leading tool results.
    rapidjson::SizeType start_idx = total_messages - static_cast<rapidjson::SizeType>(msg_left);
    while (start_idx < total_messages)
    {
        const rapidjson::Value &m = messages[start_idx];
        if (!(m.IsObject() && m.HasMember("role") && m["role"].IsString() &&
            std::string(m["role"].GetString()) == "tool"))
            break;
        ++start_idx;
    }
    for (rapidjson::SizeType i = start_idx; i < total_messages; ++i)
    {
        rapidjson::Value msg_copy;
        msg_copy.CopyFrom(messages[i], alloc);
        new_messages.PushBack(msg_copy, alloc);
    }
    
    // Replace the messages array
    messages.Clear();
    for (rapidjson::SizeType i = 0; i < new_messages.Size(); ++i)
    {
        messages.PushBack(new_messages[i], alloc);
    }
}
