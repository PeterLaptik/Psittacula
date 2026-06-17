#ifndef  CONTEXT_INFO_INCLUDED_H
#define CONTEXT_INFO_INCLUDED_H

class ChunkCompletionProcessor;

class ContextInfo
{
    public:
        ContextInfo() = default;

        explicit ContextInfo(int ctx_size);

        virtual ~ContextInfo() = default;

        void ChangeContextSize(int ctx_size);

        void Reset();
        
        void UpdateTokens(const ChunkCompletionProcessor &chunk_processor);

        /// Outputs statistic (tokens used, context)
        void WriteStat() const;

    private:
        int m_ctx_size = -1;

        int m_prompt_tokens = 0;
        int m_completion_tokens = 0;
        int m_total_tokens = 0;

        double m_tokens_cost = 0.0;

};

#endif // CONTEXT_INFO_INCLUDED_H
