#include "ai_client_impl.h"
#include "health_rsp.h"
#include "completion_rsp.h"
#include "chunk_completion_processor.h"
#include "response_readers.h"
#include "tool_factory.h"
#include "console_writer.h"
#include "default_rule_provider.h"
#include <iostream>
#include <curl/curl.h>


const std::string kEndPointHealth = "/health";
const std::string kEndPointCompletions = "/v1/chat/completions";


AiClientImpl::AiClientImpl(const std::string &host_and_port)
    : m_http_client(host_and_port)
{ 
    SetServerType(AiServerType::LlamaCPP);
    InitTools();

    DefaultRuleProvider provider;
    m_body_obj.AddSystemMessage(provider.GetDefaultSystemPrompt());
}

AiClientImpl::AiClientImpl(const std::string &host, int port)
    : m_http_client(host, port)
{
    SetServerType(AiServerType::LlamaCPP);
    InitTools();

    DefaultRuleProvider provider;
    m_body_obj.AddSystemMessage(provider.GetDefaultSystemPrompt());
}

AiClientImpl::~AiClientImpl()
{ }

bool AiClientImpl::CheckHealth()
{
    std::string response = "";
    try
    {
        response = m_http_client.HttpGet(kEndPointHealth);
        ServerHealthResponse response_obj = health_response_from_json(response);
        if (auto val = std::get_if<HealthOkResponse>(&response_obj)) {
            std::cout << "Server status: OK" << std::endl;
            return true;
        }

        if (auto val = std::get_if<HealthErrorResponse>(&response_obj)) {
            std::cout << "Server status error: ";
            std::cout << val->error.code;
            std::cout << " " << val->error.message;
            std::cout << " (" << val->error.type << ")" << std::endl;
            return false;
        }
    }
    catch (const std::runtime_error &e)
    {
        std::cout << "Response: " << response << std::endl;
        std::cout << e.what() << std::endl;
    }

    return false;
}

void AiClientImpl::SetAgentRules(const std::string &rules)
{
    m_body_obj.AddSystemMessage(rules);
}

void AiClientImpl::SendUserMessage(const std::string &message)
{
    m_body_obj.AddUserMessage(message);

    std::string body = m_body_obj.ToJsonString();

    ChunkCompletionProcessor proc;
    proc.SetReasoning(m_show_reasoning);
    m_http_client.HttpPostStream(kEndPointCompletions, body, &proc);

    int used, total;
    bool is_full;
    proc.GetStat(used, total, is_full);
    //console::write_line(fmt.Format("Tokens used %? of %?. Is context full: %?", used, total, is_full));

    std::string response_msg = proc.GetResponseMessage();
    m_body_obj.AddResponse(response_msg);

    if (proc.HasErrors())
    {
        console::write_line("\nError: " + proc.GetError(), console::TextOrigin::error);
        return;
    }

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

void AiClientImpl::RegisterTool(ToolBase *tool)
{
    auto it = m_tools_dispatcher.find(tool->GetToolName());
    if (it != m_tools_dispatcher.end())
    {
        console::write_line("\nRegister tool error: Tool exists" + tool->GetToolName(), console::TextOrigin::error);
        return;
    }

    m_body_obj.RegisterTool(tool);
    m_tools_dispatcher.insert(std::pair(tool->GetToolName(), tool));
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

void AiClientImpl::SetServerType(AiServerType type)
{
    // TODO if necessary
}

void AiClientImpl::ToolUndo()
{
    m_history_mgr.Undo();
}

void AiClientImpl::ToolRedo()
{
    m_history_mgr.Redo();
}

std::string AiClientImpl::GetDialogueBody()
{
    return m_body_obj.ToJsonString();
}

void AiClientImpl::InitTools()
{
    std::vector<ToolBase*> tools;
    get_all_tools(tools);
    for (auto tool : tools)
    {
        RegisterTool(tool);
    }
}

ToolResponse AiClientImpl::EvokeTool(ToolCall &call)
{
    ToolResponse rsp;
    rsp.role = "tool";
    rsp.name = call.name;

    auto it = m_tools_dispatcher.find(call.name);
    if (it == m_tools_dispatcher.end())
    {
        console::write_line("\nNo tool found:" + call.name, console::TextOrigin::error);
        std::string msg = fmt.Format("{\"error\": \"%?\"}", rsp.name);
        return rsp;
    }

    std::vector<ToolParameter> parameters;
    for (auto &param : call.arguments)
    {
        ToolParameter p;
        p.name = param.first;
        p.value = param.second;
        parameters.push_back(p);
    }

    auto *tool = it->second->Clone();
    std::string rsp_content = m_history_mgr.Execute(std::unique_ptr<ToolBase>(tool), parameters);
    rsp.content = rsp_content;

    return rsp;
}

void AiClientImpl::SendToolsResponses(const std::vector<ToolResponse> &responses)
{
    if (responses.empty())
        return;

    m_body_obj.AddToolResponses(responses);

    std::string body = m_body_obj.ToJsonString();

    ChunkCompletionProcessor proc;
    proc.SetReasoning(m_show_reasoning);
    m_http_client.HttpPostStream(kEndPointCompletions, body, &proc);

    int used, total;
    bool is_full;
    proc.GetStat(used, total, is_full);

    if (proc.HasErrors())
    {
        console::write_line("Error: " + proc.GetError(), console::TextOrigin::error);
        return;
    }

    std::string response_msg = proc.GetResponseMessage();
    m_body_obj.AddResponse(response_msg);

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

    SendToolsResponses(secondary_responses);
}

