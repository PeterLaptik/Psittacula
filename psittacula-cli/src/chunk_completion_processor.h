#ifndef CHUNK_COMPLETION_PROCESOR_INCLUDED_H
#define CHUNK_COMPLETION_PROCESOR_INCLUDED_H

#include "chunk_processor.h"
#include "tool_base.h"
#include <string>
#include <vector>

/// Stream response chunks receiver.
/// Parses each chunk and extracts data: messages, reasoning, tool calls, etc.
/// Outputs reasoning and system messages to a console, accumulates response message text and tool calls.
class ChunkCompletionProcessor: public ChunkProcessor
{
    public:
        ChunkCompletionProcessor() = default;

        virtual ~ChunkCompletionProcessor() = default;

        /// Clears all data from the last response
        void Reset();

        /// Process a chenk data: extract message, reasoning, tools, errors, etc
        void ProcessChunk(const std::string &chunk) override;

        /// Shows/ hides reasoning text
        void SetReasoning(bool is_shown);

        /// Returns full response message
        std::string GetResponseMessage() const;

        /// Returns full response reasoning text
        std::string GetResponseReasoning() const;

        /// Outputs statistic to a console (tokens used, context)
        void WriteStat(std::string slots_info_rsp, int context_size = -1) const;

        /// Returns whether a server returned error message for a request
        bool HasErrors() const;

        /// Returns server error text
        std::string GetError() const;

        /// Returns all tool calls from a response
        void GetResponseTools(std::vector<ToolCall> &calls_acc);

        /// Returns token statistic after response
        void GetTokensStat(int &total, int &completion, int &prompt, double &cost) const;

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

        std::string m_message;      // message text accumulator
        std::string m_reasoning;    // reasoning text accumulator

        int m_total_tokens = 0;
        int m_completion_tokens = 0;
        int m_prompt_tokens = 0;
        int m_tokens_cost = 0;

        // Tools
        struct FunctionToEvoke
        {
            std::string id;
            std::string name;
            std::string arguments;
        };

        FunctionToEvoke m_current_tool;         // current tool, building by deltas
        std::vector<FunctionToEvoke> m_tools;   // all tool lists

        std::string m_error; // error message
};

#endif // CHUNK_COMPLETION_PROCESOR_INCLUDED_H