#include "ai_client_impl.h"
#include "chunk_completion_processor.h"
#include "response_readers.h"
#include "tool_factory.h"
#include "console_writer.h"
#include "default_rule_provider.h"
#include <iostream>
#include <algorithm>
#include <curl/curl.h>

// Maximum number of tool calls per response, to prevent infinite loops in case of errors
const int kToolsCallMax = 20; 

const std::string kEndPointHealth = "/health";
const std::string kEndPointCompletions = "/chat/completions";
const std::string kEndPointSlots = "/slots";


AiClientImpl::AiClientImpl(const std::string &host_and_port, int context_size)
    : m_http_client(host_and_port), m_context_size(context_size)
{ 
    InitTools();

    DefaultRuleProvider provider;
    m_body_obj.AddSystemMessage(provider.GetDefaultSystemPrompt());
}

AiClientImpl::AiClientImpl(const std::string &host, int port, int context_size)
    : m_http_client(host, port), m_context_size(context_size)
{
    InitTools();

    DefaultRuleProvider provider;
    m_body_obj.AddSystemMessage(provider.GetDefaultSystemPrompt());
}

AiClientImpl::~AiClientImpl()
{ }

void AiClientImpl::SendUserMessage(const std::string &message)
{
    m_tool_loop_counter = 0;

    // Add message to a dialogue body and send to a server
    m_body_obj.AddUserMessage(message);

    std::string body = m_body_obj.ToJsonString();

    ChunkCompletionProcessor proc; // POST response chunk receiver
    proc.SetReasoning(m_show_reasoning);
    m_http_client.HttpPostStream(kEndPointCompletions, body, &proc);

    std::string slots_rsp = GetSlotstInfo();
    proc.ShowStat(slots_rsp, m_context_size);

    // Add response message to a dialogue body
    std::string response_msg = proc.GetResponseMessage();
    m_body_obj.AddResponse(response_msg);

    // Check errors
    if (proc.HasErrors())
    {
        console::write_line("\nError: " + proc.GetError(), console::TextOrigin::error);
        return;
    }

    // Check and process tool calls if exist
    std::vector<ToolCall> tool_calls;
    proc.GetResponseTools(tool_calls);

    std::vector<ToolResponse> responses;
    for (auto &tool_call : tool_calls)
    {
        ToolResponse rsp = EvokeTool(tool_call);
        responses.push_back(rsp);
    }

    SendToolsResponses(responses);
}

void AiClientImpl::RegisterTool(std::unique_ptr<ToolBase> tool)
{
    auto it = m_tools_dispatcher.find(tool->GetToolName());
    if (it != m_tools_dispatcher.end())
    {
        console::write_line("\nRegister tool error: tool exists" + tool->GetToolName(), console::TextOrigin::error);
        return;
    }

    m_body_obj.RegisterTool(tool.get());
    m_tools_dispatcher.insert(std::pair(tool->GetToolName(), std::move(tool)));
}

void AiClientImpl::GetToolsInfo(std::vector<std::pair<std::string, std::string>> &tools_acc)
{
    for (auto &tool : m_tools_dispatcher)
    {
        std::string name = tool.first;
        std::string purpose = tool.second.get()->GetToolDescription();
        tools_acc.push_back(std::make_pair(name, purpose));
    }
}

void AiClientImpl::InitTools()
{
    std::vector<std::unique_ptr<ToolBase>> tools;
    get_all_tools(tools);
    for (auto &tool : tools)
    {
        RegisterTool(std::move(tool));
    }
}

ToolResponse AiClientImpl::EvokeTool(ToolCall &call)
{
    ToolResponse rsp;
    rsp.id = call.id;
    rsp.role = "tool";
    rsp.name = call.name;
    rsp.input_content = call.content;

    // Find tool in a dispatcher
    auto it = m_tools_dispatcher.find(call.name);
    if (it == m_tools_dispatcher.end())
    {
        console::write_line("\nNo tool found:" + call.name, console::TextOrigin::error);
        std::string msg = fmt.Format("{\"error\": \"%?\"}", rsp.name);
        return rsp;
    }

    // Set tool's arguments
    std::vector<ToolParameter> parameters;
    for (auto &param : call.arguments)
    {
        ToolParameter p;
        p.name = param.first;
        p.value = param.second;
        parameters.push_back(p);
    }

    // Evoke tool and get response
    auto *tool = it->second->Clone();
    std::string rsp_content = m_history_mgr.Execute(std::unique_ptr<ToolBase>(tool), parameters);
    rsp.output_content = rsp_content;

    return rsp;
}

void AiClientImpl::SendToolsResponses(const std::vector<ToolResponse> &tools_responses, const std::string &response)
{
    if (tools_responses.empty())
        return;

    // Infinit loop check
    m_tool_loop_counter++;
    if (m_tool_loop_counter > kToolsCallMax)
    {
        console::write_line("\nError: Too many tool calls loops, possible infinite loop. Agent stopped the calls.", console::TextOrigin::error);
        return;
    }

    // Send tool responses and get result message from LLM
    m_body_obj.AddToolResponses(tools_responses);

    std::string body = m_body_obj.ToJsonString();

    ChunkCompletionProcessor proc; // POST response chunks receiver
    proc.SetReasoning(m_show_reasoning);
    m_http_client.HttpPostStream(kEndPointCompletions, body, &proc);

    std::string slots_rsp = GetSlotstInfo();
    proc.ShowStat(slots_rsp, m_context_size);

    if (proc.HasErrors())
    {
        console::write_line("Error: " + proc.GetError(), console::TextOrigin::error);
        return;
    }

    // Response message after tools execution
    std::string response_msg = proc.GetResponseMessage();
    bool is_not_empty_message = m_body_obj.AddResponse(response_msg);

    if(is_not_empty_message)
        m_tool_loop_counter = 0; // Reset tool loop counter for a non-empty message

    // Execute new tool calls, if there are any
    std::vector<ToolCall> tool_calls;
    proc.GetResponseTools(tool_calls);
    if (tool_calls.empty())
        return;

    std::vector<ToolResponse> secondary_responses;
    for (auto &tool_call : tool_calls)
    {
        ToolResponse rsp = EvokeTool(tool_call);
        secondary_responses.push_back(rsp);
    }

    // Recursive call to send secondary tool responses
    SendToolsResponses(secondary_responses);
}

void AiClientImpl::SetReasoning(bool is_shown)
{
    m_show_reasoning = is_shown;
}

void AiClientImpl::SetApiKey(const std::string &key)
{
    m_http_client.SetApiKey(key);
}

void AiClientImpl::SetModel(const std::string &model)
{
    m_body_obj.SetModel(model);
}

void AiClientImpl::SetAgentRules(const std::string &rules)
{
    m_body_obj.AddSystemMessage(rules);
}

std::string AiClientImpl::GetSlotstInfo()
{
    return m_http_client.HttpGet(kEndPointSlots);
}

void AiClientImpl::ClearContext()
{
    m_body_obj.ClearContext();
}

void AiClientImpl::RestoreDialogueFrom(const std::string &data)
{
    m_body_obj.FromJsonString(data);
}

void AiClientImpl::ToolUndo()
{
    m_history_mgr.Undo();
}

void AiClientImpl::ToolRedo()
{
    m_history_mgr.Redo();
}

std::string AiClientImpl::GetDialogueBody() const
{
    return m_body_obj.ToJsonString();
}

std::string AiClientImpl::GetAgentRules() const
{
    return m_body_obj.GetSystemMessage();
}