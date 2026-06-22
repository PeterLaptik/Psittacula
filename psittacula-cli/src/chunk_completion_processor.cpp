#include "chunk_completion_processor.h"
#include "console_writer.h"
#include "format_util.h"
#include <sstream>
#include <iomanip>
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


// Resoning progress
// Shows rotating line in a console
static size_t progress_cursor = 0;
static const char progress[4] = { '/', '|', '\\', '-' };

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

    // Is response finished?
    if (json == " [DONE]" || json == "[DONE]")
    {
        return;
    }
        

    rapidjson::Document doc;
    if (!doc.Parse(json.c_str()).HasParseError())
    {
        JsonDocument data{doc};

        // Try to process data
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
        // Check if the full uncut chunk can be parsed: get server errors
        JsonDocument data{doc};
        CheckErrors(data);
    }
    else
    {
        // Skip: ignore unsupported chunks
    }
}

void ChunkCompletionProcessor::WriteStat(std::string slots_info_rsp, int context_size) const
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

    double ratio_ctx = context_size > 0 ? static_cast<double>(m_total_tokens) / static_cast<double>(context_size) : 0;
    double percentage_ctx = std::round(ratio_ctx * 10000) / 100;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << percentage_ctx;
    std::string percentage_ctx_str = oss.str();

    std::string context_usage_str = context_size > 0 ?
        percentage_ctx_str + "% of context (" + std::to_string(context_size) + ")" : "";

    console::write_line("\nTokens: " + std::to_string(m_total_tokens)
        + " (prompt: " + std::to_string(m_prompt_tokens) +
        + " / completion: " + std::to_string(m_completion_tokens)
        + ") \t"
        + context_usage_str
        + (m_tokens_cost > 0 ? "cost: " + std::to_string(m_tokens_cost) : "")
        + "\n", console::TextOrigin::reasoning);
    console::write_splitter();
}

void ChunkCompletionProcessor::CheckMessage(JsonDocument &doc)
{
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

    const auto &delta = doc.body["choices"][0]["delta"];

    // ========== Output reasoning data ====================
    // Reasoning text or progress
    // Ignored after the first non-empty contet (see the code above)
    // Checking fields: reasoning_content or reasoning
    // The first condition works for llama.cpp
    if (delta.HasMember("reasoning_content") && delta["reasoning_content"].IsString()) {
        std::string reasoning_txt = delta["reasoning_content"].GetString();
        OutputReasoning(reasoning_txt);
        m_reasoning += reasoning_txt;
        m_reasoning_in_process = true;
    }
    // The condition works for other servers
    else if (delta.HasMember("reasoning") && delta["reasoning"].IsString())
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
        if (usage.HasMember("cost") && usage["cost"].IsDouble())
        {
            m_tokens_cost = usage["cost"].IsDouble();
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
                    std::cout << "ID ========== " << id << std::endl;
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

        // Assembling th full content like JSON stringified object:
        // {\"arg1\": 1, \"arg2\": \"2str\", etc.}
        std::string full_content;

        for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) 
        {
            std::string key = it->name.GetString();
            full_content += fmt.Format("\"%?\":", key);

            const rapidjson::Value &value = it->value;

            std::string arg_val;
            if (value.IsString()) {
                arg_val = value.GetString();
                full_content += fmt.Format("\"%?\"", arg_val);
            }
            else if (value.IsInt()) {
                arg_val = std::to_string(value.GetInt());
                full_content += fmt.Format("%?", arg_val);
            }
            else if (value.IsBool()) {
                arg_val = std::to_string(value.GetBool());
                full_content += fmt.Format("%?", arg_val);
            }
            else
            {
                std::string msg = fmt.Format("GetTools: Unknown type of argument: %?", key);
                console::write_line(msg, TextOrigin::error);
            }

            caller.arguments.emplace_back(key, arg_val);
            full_content += ",";
        }

        if (!full_content.empty())
        {
            full_content.pop_back(); // remove last comma
            full_content += "}";
        }
        else
        {
            full_content = "{}"; // no args for call
        }

        full_content = "{" + full_content; // {\"arg1\": 1, \"arg2\": \"2str\", etc.}

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
