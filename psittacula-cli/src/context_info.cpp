#include "context_info.h"

ContextInfo::ContextInfo(int ctx_size)
    : m_ctx_size(ctx_size)
{ }

void ContextInfo::ChangeContextSize(int ctx_size)
{
    m_ctx_size = ctx_size;
}

void ContextInfo::Reset()
{
    m_completion_tokens = 0;
    m_prompt_tokens = 0;
    m_total_tokens = 0;
    m_tokens_cost = 0;
}

void ContextInfo::WriteStat() const
{
}
