#include "model.h"
#include "clients_factory.h"
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>

Model::Model(const std::string &name, const std::string &host, const std::string api_key, int context_size)
    : m_name(name), m_host(host), m_api_key(api_key), m_context_size(context_size)
{ }

Model Model::FromFile(const std::string &file_path)
{
    std::map<std::string, std::string> params;
    std::ifstream file(file_path);

    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + file_path);

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);

        if (line.empty() || line[0] == '#')
            continue;

        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;
        auto tmp = line.substr(0, pos);
        std::string key = Trim(line.substr(0, pos));
        std::string value = Trim(line.substr(pos + 1));

        params[key] = value;
    }

    std::string model_name = params.count("name") ? params["name"] : "UnnamedModel";
    std::string host = params.count("host") ? params["host"] : "";
    std::string api_key = params.count("api_key") ? params["api_key"] : "";
    std::string context_size_str = params.count("api_key") ? params["context_size"] : "-1";

    int context_size = -1;
    try
    {
        context_size = std::stoi(context_size_str);
    }
    catch (...) {
        // ignore errors
    }

    return Model(model_name, host, api_key, context_size);
}

std::string Model::GetName() const
{
    return m_name;
}

std::string Model::GetHost() const
{
    return m_host;
}

std::string Model::GetApiKey() const
{
    return m_api_key;
}

int Model::GetContextSize() const
{
    return m_context_size;
}

AiClient *Model::GetClient() const
{
    AiClient *client = create_base_client(m_host, m_context_size);
    client->SetApiKey(m_api_key);
    client->SetModel(m_name);
    // Note: Setting context size on the client is not supported by the current AI client interface.
    // If needed in the future, we can add a SetContextSize method to AiClient.
    return client;
}

inline std::string Model::Trim(const std::string &line)
{
    auto start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) 
        return "";

    auto end = line.find_last_not_of(" \t\r\n");

    return 
        line.substr(start, end - start + 1);
}
