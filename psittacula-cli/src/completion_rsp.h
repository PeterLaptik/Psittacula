#ifndef RSP_COMPLETION_INCLDED_H
#define RSP_COMPLETION_INCLDED_H

#include "tool_base.h"
#include <string>
#include <vector>

struct ToolExecutionCall
{
    std::string tool_name;
    std::vector<ToolParameter> tool_parameters;
};

struct CompletionResponse
{
    std::string message;

    std::vector<ToolExecutionCall> tool_calls;

    int total_tokens = 0;
    int completion_tokens = 0;
    int prompt_tokens = 0;
    double tokens_cost = 0;

    bool is_error = false;
};

#endif // RSP_COMPLETION_INCLDED_H