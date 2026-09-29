#include "chunk_completion_processor.h"
#include "console_writer.h"
#include "format_util.h"
#include <random>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

using console::TextOrigin;

struct ChunkCompletionProcessor::JsonDocument
{
    rapidjson::Document &body;
};

void ChunkCompletionProcessor::Reset()
{
    m_reasoning_in_process = true;
    m_show_reasoning = true;

    m_message.clear();
    m_reasoning.clear();

    m_total_tokens = 0;
    m_completion_tokens = 0;
    m_prompt_tokens = 0;
    m_tokens_cost = 0;

    m_current_tool.name.clear();
    m_current_tool.arguments.clear();
    m_tools.clear();
}

void ChunkCompletionProcessor::ProcessChunk(const std::string &chunk)
{
    if (chunk.size() < 5)
        return;

    // Cut 'data:' text, leave JSON body only
    std::string json = chunk.substr(5);

    // Trim
    json.erase(0, json.find_first_not_of(" \t\n\r\f\v"));
    json.erase(json.find_last_not_of(" \t\n\r\f\v") + 1);

    // Is response finished? (json is already trimmed)
    if (json == "[DONE]")
    {
        return;
    }
        

    rapidjson::Document doc;
    if (!doc.Parse(json.c_str()).HasParseError())
    {
        JsonDocument data{doc};

        // Try to process data
        if (doc.HasMember("choices") && doc["choices"].Size() > 0 && doc["choices"][0].HasMember("delta"))
        {
            CheckReasoning(data);
            CheckMessage(data);
            CheckTools(data);
        }

        // Usage/cost data can arrive in a trailing chunk with an empty
        // "choices" array (e.g. Kilo Gateway, OpenRouter, OpenAI with
        // stream_options.include_usage), so check it unconditionally.
        CheckTokens(data);
    }
    else if(!doc.Parse(chunk.c_str()).HasParseError())
    {
        // Check if the full uncut chunk can be parsed: get server errors
        JsonDocument data{doc};
        CheckErrors(data);
    }
    else
    {
        // Skip: ignore unsupported chunks
    }

    console::move_spinner();
}

void ChunkCompletionProcessor::ShowStat(std::string slots_info_rsp, int context_size) const
{
    // Trying to get context from /slots endpoint response
    // If it is not well formed / does not exist, use 'context_size' value
    rapidjson::Document doc;
    doc.Parse(slots_info_rsp.c_str());

    if (!doc.HasParseError() && doc.IsArray() && !doc.Empty())
    {
        const auto &firstSlot = doc[0];

        if (firstSlot.IsObject() &&
            firstSlot.HasMember("n_ctx") &&
            firstSlot["n_ctx"].IsInt())
        {
            context_size = firstSlot["n_ctx"].GetInt();
        }
    }

    console::write_status(this, context_size);
}

void ChunkCompletionProcessor::CheckMessage(JsonDocument &doc)
{
    // Final chunks (e.g. finish_reason only) and some providers may omit 'delta'
    if (!doc.body["choices"][0].IsObject() || !doc.body["choices"][0].HasMember("delta"))
        return;

    // ========== Content output ==========================
    const auto &delta = doc.body["choices"][0]["delta"];

    if (delta.HasMember("content") && delta["content"].IsString())
    {
        // Empty content can exist during reasoning
        // So the first non-empty value is considered as a finished reasoning, see the check below
        size_t c_len = std::strlen(delta["content"].GetString());

        // Reset reasoning mode output after first entering
        if (m_reasoning_in_process && c_len > 0)
        {
            m_reasoning_in_process = false;
            console::write_line("");
            console::write_splitter();
            console::flush();
        }

        console::write(delta["content"].GetString(), TextOrigin::machine);
        console::flush();
        m_message += delta["content"].GetString();
    }
}

void ChunkCompletionProcessor::CheckReasoning(JsonDocument &doc)
{
    if (!m_reasoning_in_process)
        return;

    // Final chunks (e.g. finish_reason only) and some providers may omit 'delta'
    if (!doc.body["choices"][0].IsObject() || !doc.body["choices"][0].HasMember("delta"))
        return;

    const auto &delta = doc.body["choices"][0]["delta"];

    // ========== Output reasoning data ====================
    // Reasoning text or progress
    // Ignored after the first non-empty content (see the code above)
    // Checking fields, in order: reasoning_content, reasoning, thinking
    //  - "reasoning_content" works for llama.cpp / DeepSeek-style servers
    //  - "reasoning" works for OpenRouter and similar normalized servers
    //  - "thinking" works for some other OpenAI-compatible providers/proxies
    static const char *reasoning_fields[] = { "reasoning_content", "reasoning", "thinking" };

    for (const char *field : reasoning_fields)
    {
        if (delta.HasMember(field) && delta[field].IsString())
        {
            std::string reasoning_txt = delta[field].GetString();
            OutputReasoning(reasoning_txt);
            m_reasoning += reasoning_txt;
            m_reasoning_in_process = true;
            break;
        }
    }
}

void ChunkCompletionProcessor::CheckTokens(JsonDocument &doc)
{
    // ========== Tokens counter ==========================
    // The condition works for llama.cpp
    if (doc.body.HasMember("timings"))
    {
        const auto &tokens = doc.body["timings"];
        if (tokens.HasMember("predicted_n") && tokens["predicted_n"].IsInt())
        {
            m_completion_tokens = tokens["predicted_n"].GetInt();
        }
        int ctx_cached = 0;
        if (tokens.HasMember("cache_n") && tokens["cache_n"].IsInt())
        {
            ctx_cached = tokens["cache_n"].GetInt();
        }
        if (tokens.HasMember("prompt_n") && tokens["prompt_n"].IsInt())
        {
            m_prompt_tokens = tokens["prompt_n"].GetInt();
        }

        m_total_tokens = ctx_cached + m_prompt_tokens + m_completion_tokens;
    }

    // The condition works for other servers
    if (doc.body.HasMember("usage"))
    {
        const auto &usage = doc.body["usage"];
        if (usage.HasMember("total_tokens") && usage["total_tokens"].IsInt())
        {
            m_total_tokens = usage["total_tokens"].GetInt();
        }
        if (usage.HasMember("completion_tokens") && usage["completion_tokens"].IsInt())
        {
            m_completion_tokens = usage["completion_tokens"].GetInt();
        }
        if (usage.HasMember("prompt_tokens") && usage["prompt_tokens"].IsInt())
        {
            m_prompt_tokens = usage["prompt_tokens"].GetInt();
        }
        if (usage.HasMember("cost") && usage["cost"].IsNumber())
        {
            double read_cost = usage["cost"].GetDouble();
            m_tokens_cost = read_cost > m_tokens_cost ? read_cost : m_tokens_cost;
        }
    }
}

void ChunkCompletionProcessor::CheckTools(JsonDocument &doc)
{
    // Final chunks (e.g. finish_reason only) and some providers may omit 'delta'
    if (!doc.body["choices"][0].IsObject() || !doc.body["choices"][0].HasMember("delta"))
        return;

    const auto &delta = doc.body["choices"][0]["delta"];

    // ========== Tools calling ===========================
    if (delta.HasMember("tool_calls") && delta["tool_calls"].IsArray())
    {
        for (auto &t : delta["tool_calls"].GetArray())
        {
            // ---- Function object ----
            if (t.HasMember("function") && t["function"].IsObject())
            {
                const auto &fn = t["function"];

                std::string id;
                if (fn.HasMember("id") && fn["id"].IsString())
                {
                    id = fn["id"].GetString();
                }

                std::string name;
                if (fn.HasMember("name") && fn["name"].IsString())
                    name = fn["name"].GetString();

                if (!name.empty()) // Next tool call: new id? May be wrong! Recheck in the future!
                {
                    // Push current tool to the a list and fill a new tool data
                    if (!m_current_tool.name.empty())
                    {
                        m_tools.push_back(m_current_tool);
                    }
                    m_current_tool.id = id;
                    m_current_tool.name = name;
                    m_current_tool.arguments.clear();
                }
                else
                {
                    if(!id.empty())
                        m_current_tool.id += id;
                }

                if (fn.HasMember("arguments") && fn["arguments"].IsString())
                    m_current_tool.arguments += fn["arguments"].GetString();
            }
        }
    }
}

void ChunkCompletionProcessor::CheckErrors(JsonDocument &doc)
{
    // Some servers return 'error' as a plain string, others as an object
    if (doc.body.HasMember("error") && doc.body["error"].IsString())
    {
        std::string msg = doc.body["error"].GetString();
        m_error = msg.empty() ? "Server error. Unknown error occurred." : "Server error. " + msg;
        return;
    }

    if (doc.body.HasMember("error") && doc.body["error"].IsObject())
    {
        const auto &err = doc.body["error"];
        if (err.HasMember("message") && err["message"].IsString())
        {
            m_error = "Server error. " + std::string(err["message"].GetString());
        }
        else
        {
            m_error = "Server error. Unknown error occurred.";
        }
    }
}

void ChunkCompletionProcessor::OutputSystemMessage(const std::string &msg)
{
    console::write(msg, TextOrigin::tools);
}

void ChunkCompletionProcessor::OutputReasoning(const std::string &msg) const
{
    if (m_show_reasoning)
    {
        console::write(msg, TextOrigin::reasoning);
        console::flush();
    }
}

std::string ChunkCompletionProcessor::GetError() const
{
    return m_error;
}

void ChunkCompletionProcessor::GetResponseTools(std::vector<ToolCall> &calls_acc)
{
    static std::mt19937 rng(std::random_device{}());

    Formatter fmt;
    if (!m_current_tool.name.empty())
        m_tools.push_back(m_current_tool);

    for (FunctionToEvoke &fn : m_tools)
    {
        ToolCall caller;
        caller.name = fn.name;
        caller.id = fn.id;

        if (caller.id.empty())
        {
            std::uniform_int_distribution<int> dist(1, std::numeric_limits<int>::max());
            caller.id = "tool_call_" + std::to_string(dist(rng)); // Autogenerated id: tool_call_XXXXX
        }

        rapidjson::Document doc;
        bool has_errors = doc.Parse(fn.arguments.c_str()).HasParseError();
        if (has_errors)
        {
            console::write_line("Tool call: JSON parse error\n", TextOrigin::error);
            console::write_line(fn.arguments, TextOrigin::error);
            continue;
        }

        if (!doc.IsObject()) 
        {
            console::write_line("Tool call error: " + caller.name, TextOrigin::error);
            console::write_line("Expected JSON object\n", TextOrigin::error);
            continue;
        }

        // Iterate members: fill parsed arguments for the tool call.
        // caller.content is the raw arguments JSON (see below).
        for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it)
        {
            std::string key = it->name.GetString();
            const rapidjson::Value &value = it->value;

            std::string arg_val;
            if (value.IsString()) {
                arg_val = value.GetString();
            }
            else if (value.IsInt()) {
                arg_val = std::to_string(value.GetInt());
            }
            else if (value.IsInt64()) {
                arg_val = std::to_string(value.GetInt64());
            }
            else if (value.IsUint()) {
                arg_val = std::to_string(value.GetUint());
            }
            else if (value.IsUint64()) {
                arg_val = std::to_string(value.GetUint64());
            }
            else if (value.IsDouble() || value.IsFloat()) {
                arg_val = std::to_string(value.GetDouble());
            }
            else if (value.IsBool()) {
                arg_val = value.GetBool() ? "true" : "false";
            }
            else {
                // Nested arrays/objects: serialize back to a JSON string
                rapidjson::StringBuffer buf;
                rapidjson::Writer<rapidjson::StringBuffer> buf_writer(buf);
                if (value.Accept(buf_writer))
                    arg_val = buf.GetString();
            }

            if (arg_val.empty() && !value.IsNull())
            {
                std::string msg = fmt.Format("GetTools: Unsupported type of argument: %?", key);
                console::write_line(msg, TextOrigin::error);
            }

            // JSON null is passed as an empty value
            caller.arguments.emplace_back(key, arg_val);
        }

        // Raw arguments as a content
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        caller.content = buffer.GetString();

        calls_acc.push_back(caller);
    }
}

void ChunkCompletionProcessor::GetTokensStat(int &total, int &completion, int &prompt, double &cost) const
{
    total = m_total_tokens;
    completion = m_completion_tokens;
    prompt = m_prompt_tokens;
    cost = m_tokens_cost;
}

void ChunkCompletionProcessor::SetShowReasoning(bool is_shown)
{
    m_show_reasoning = is_shown;
}

std::string ChunkCompletionProcessor::GetResponseMessage() const
{
    return m_message;
}

std::string ChunkCompletionProcessor::GetResponseReasoning() const
{
    return m_reasoning;
}

void ChunkCompletionProcessor::SetCancelFlag(std::atomic<bool> *flag)
{
    m_cancel_flag = flag;
}

bool ChunkCompletionProcessor::IsCancelled() const
{
    return m_cancel_flag && m_cancel_flag->load();
}

bool ChunkCompletionProcessor::HasErrors() const
{
    return !m_error.empty();
}
