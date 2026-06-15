#ifndef CHUNK_COMPLETION_PROCESOR_INCLUDED_H
#define CHUNK_COMPLETION_PROCESOR_INCLUDED_H

#include "chunk_processor.h"
#include "tool_base.h"
#include <string>
#include <vector>

class ChunkCompletionProcessor: public ChunkProcessor
{
    public:
        ChunkCompletionProcessor() = default;

        virtual ~ChunkCompletionProcessor() = default;

        void Reset();

        void ProcessChunk(const std::string &chunk) override;

        void SetReasoning(bool is_shown);

        std::string GetResponseMessage() const;

        std::string GetResponseReasoning() const;

        void WriteStat(std::string ctx_data, int context_size = -1) const;

        bool HasErrors() const;

        std::string GetError() const;

        void GetStat(int &used, int &total, bool &is_full) const;

        void GetResponseTools(std::vector<ToolCall> &calls_acc);

    private:
        void OutputSystemMessage(const std::string &msg);
        void OutputReasoning(const std::string &msg) const;

        struct JsonDocument;
        void CheckMessage(JsonDocument &doc);
        void CheckReasoning(JsonDocument &doc);
        void CheckTokens(JsonDocument &doc);
        void CheckTools(JsonDocument &doc);
        void CheckErrors(JsonDocument &doc);

        bool m_reasoning_in_process = true;
        bool m_show_reasoning = false;
        bool m_is_reading_function = false;

        std::string m_message;
        std::string m_reasoning;

        int m_total_tokens = 0;
        int m_completion_tokens = 0;
        int m_prompt_tokens = 0;
        int m_tokens_cost = 0;

        int m_kv_used = 0;
        int m_kv_total = 0;
        bool m_context_full = false;

        // Tools
        std::string tool_delta;

        struct FunctionToEvoke
        {
            std::string id;
            std::string name;
            std::string arguments;
        };
        FunctionToEvoke m_current_tool;

        std::vector<FunctionToEvoke> m_tools;

        std::string m_error;

};

#endif // CHUNK_COMPLETION_PROCESOR_INCLUDED_H