#ifndef MODEL_INCLUDED_H
#define MODEL_INCLUDED_H

#include "ai_client.h"
#include <string>

// Model file: extension .txt
// See example txt file
class Model final
{
    public:
        Model() = default;

        Model(const std::string &name, const std::string &host, const std::string api_key, int context_size = -1);

        ~Model() = default;

        static Model FromFile(const std::string &file_path);

        std::string GetName() const;

        std::string GetHost() const;

        std::string GetApiKey() const;

        int GetContextSize() const;

        AiClient* GetClient() const;

    private:
        static inline std::string Trim(const std::string &s);

        std::string m_name;
        std::string m_host;
        std::string m_api_key;
        int m_context_size = -1; // -1 means not set
};

#endif // !MODEL_INCLUDED_H