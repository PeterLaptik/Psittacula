#include "chunk_completion_processor.h"
#include "console_writer.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

using console::TextOrigin;

struct ChunkCompletionProcessor::JsonDocument
{
    rapidjson::Document &body;
};

static const char *kReasoningOutputColour = "\033[90m";

// Resoning progress
// Shows rotating line in a console
static size_t progress_cursor = 0;
static const char progress[4] = { '/', '|', '\\', '-' };

void ChunkCompletionProcessor::Reset()
{
    m_reasoning_in_process = true;
    m_show_reasoning = true;
    m_is_reading_function = false;

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

    std::string json = chunk.substr(5);

    // Trim
    json.erase(0, json.find_first_not_of(" \t\n\r\f\v"));
    json.erase(json.find_last_not_of(" \t\n\r\f\v") + 1);

    // Is finished?
    if (json == " [DONE]" || json == "[DONE]")
    {
        console::write_line("\nTokens: " + std::to_string(m_total_tokens) + "\n", console::TextOrigin::reasoning);
        console::write_splitter();
        return;
    }
        

    rapidjson::Document doc;
    if (!doc.Parse(json.c_str()).HasParseError())
    {
        JsonDocument data{doc};

        if (doc.HasMember("choices") && doc["choices"].Size() > 0)
        {
            CheckReasoning(data);
            CheckMessage(data);
            CheckTools(data);
            CheckTokens(data);
        }

        
    }
    else if(!doc.Parse(chunk.c_str()).HasParseError())
    {
        JsonDocument data{doc};
        CheckErrors(data);
    }
    else
    {
        // Skip
    }
}

void ChunkCompletionProcessor::SetReasoning(bool is_shown)
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

bool ChunkCompletionProcessor::HasErrors() const
{
    return !m_error.empty();
}

void ChunkCompletionProcessor::CheckMessage(JsonDocument &doc)
{
    // ========== Content output ==========================
    const auto &delta = doc.body["choices"][0]["delta"];

    if (delta.HasMember("content") && delta["content"].IsString())
    {
        // Empty content can exist, so the first non-empty value is considered as a finished reasoning, see the check below
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
    const auto &delta = doc.body["choices"][0]["delta"];

    // ========== Output reasoning data ====================
    // Reasoning text or progress
    // Ignored after the first non-empty contet (see the code above)
    // Checking fields: reasoning_content or reasoning
    // The first condition works for llama.cpp
    if (m_reasoning_in_process && delta.HasMember("reasoning_content") && delta["reasoning_content"].IsString()) {
        std::string reasoning_txt = delta["reasoning_content"].GetString();
        OutputReasoning(reasoning_txt);
        m_reasoning += reasoning_txt;
        m_reasoning_in_process = true;
    }
    // The condition works for open router
    else if (m_reasoning_in_process && delta.HasMember("reasoning") && delta["reasoning"].IsString())
    {
        std::string reasoning_txt = delta["reasoning"].GetString();
        OutputReasoning(reasoning_txt);
        m_reasoning += reasoning_txt;
        m_reasoning_in_process = true;
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
            m_total_tokens = tokens["predicted_n"].GetInt();
        }
    }

    // The condition works for OpenRouter
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
        if (usage.HasMember("cost") && usage["cost"].IsDouble())
        {
            m_tokens_cost = usage["cost"].GetInt();
        }
    }

    if (doc.body.HasMember("context"))
    {
        const auto &usage = doc.body["context"];
        if (usage.HasMember("kv_used") && usage["kv_used"].IsInt())
        {
            m_kv_used = usage["kv_used"].GetInt();
        }
        if (usage.HasMember("kv_total") && usage["kv_total"].IsInt())
        {
            m_kv_total = usage["kv_total"].GetInt();
        }
        if (usage.HasMember("context_full") && usage["context_full"].IsBool())
        {
            m_context_full = usage["context_full"].GetBool();
        }
    }
}

void ChunkCompletionProcessor::CheckTools(JsonDocument &doc)
{
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

                if (!name.empty())
                {
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

    // ===================== LEGACY function_call ====================
    // Not tested
    /*
    if (delta.HasMember("function_call") && delta["function_call"].IsObject())
    {
        const auto &fc = delta["function_call"];
        auto &entry = tool_calls["legacy_0"];
        entry.id = "legacy_0";

        if (fc.HasMember("name") && fc["name"].IsString())
            entry.name = fc["name"].GetString();

        if (fc.HasMember("arguments") && fc["arguments"].IsString())
            entry.arguments += fc["arguments"].GetString();
    }
    */
}

void ChunkCompletionProcessor::CheckErrors(JsonDocument &doc)
{
    if (doc.body.HasMember("error") && doc.body["error"].IsObject())
    {
        const auto &err = doc.body["error"];
        if (err.HasMember("message") && err["message"].IsString())
        {
            m_error = "Server error. ";
            m_error = err["message"].GetString();
        }
        else
        {
            m_error = "Server error. Unknown error occurred.";
        }
    }
}

void ChunkCompletionProcessor::OutputSystemMessage(const std::string &msg)
{
    console::write(msg, TextOrigin::filesystem);
}

void ChunkCompletionProcessor::OutputReasoning(const std::string &msg) const
{
    if (m_show_reasoning)
    {
        console::write(msg, TextOrigin::reasoning);
        console::flush();
    }
    else
    {
        progress_cursor = (progress_cursor >= 3 ? 0 : progress_cursor + 1);
        std::string r_msg = "\rReasoning ";
        r_msg += progress[progress_cursor];
        console::write(r_msg, TextOrigin::reasoning);
        if (m_total_tokens > 0)
            console::write("\ttokens: " + std::to_string(m_total_tokens), TextOrigin::reasoning);
    }
}

std::string ChunkCompletionProcessor::GetError() const
{
    return m_error;
}

void ChunkCompletionProcessor::GetStat(int &used, int &total, bool &is_full) const
{
    used = m_kv_used;
    total = m_kv_total;
    is_full = m_context_full;
}

void ChunkCompletionProcessor::GetResponseTools(std::vector<ToolCall> &calls_acc)
{
    if (!m_current_tool.name.empty())
        m_tools.push_back(m_current_tool);

    for (FunctionToEvoke &fn : m_tools)
    {
        ToolCall caller;
        caller.name = fn.name;
        caller.id = fn.id;

        rapidjson::Document doc;
        bool has_errors = doc.Parse(fn.arguments.c_str()).HasParseError();
        if (has_errors)
        {
            console::write_line("Tools: JSON parse error\n", TextOrigin::error);
            console::write_line(fn.arguments, TextOrigin::error);
            continue;
        }

        if (!doc.IsObject()) 
        {
            console::write_line("Tool: " + caller.name, TextOrigin::error);
            console::write_line("Expected JSON object\n", TextOrigin::error);
            continue;
        }

        // Raw arguments as a content
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        caller.content = buffer.GetString();

        for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
            const char *key = it->name.GetString();
            const rapidjson::Value &value = it->value;

            std::string arg_val;
            if (value.IsString()) {
                arg_val = value.GetString();
            }
            else if (value.IsInt()) {
                arg_val = std::to_string(value.GetInt());
            }
            else if (value.IsBool()) {
                arg_val = std::to_string(value.GetBool());
            }
            else
            {
                console::write("GetTools: Unknown type of argument\n", TextOrigin::error);
            }

            caller.arguments.emplace_back(key, arg_val);
        }

        calls_acc.push_back(caller);
    }
}