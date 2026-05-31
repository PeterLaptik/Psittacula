#include "param_reader.h"
#include <iostream>

ParamReader::ParamReader(char **argv, int argc)
    : m_argv(argv), m_argc(argc)
{ }

void ParamReader::ReadParams()
{
    for (int i = 1; i < m_argc; i++)
    {
        std::cout << "Processing argument: " << m_argv[i] << std::endl;
        std::string arg(m_argv[i]);
        size_t pos = arg.find('=');
        if (pos != std::string::npos)
        {
            std::string key = arg.substr(0, pos);
            std::string value = arg.substr(pos + 1);
            params[key] = value;
        }
        else
        {
            m_flags.push_back(arg);
        }
    }
}

std::string ParamReader::GetParam(const std::string &key, const std::string &default_value) const
{
    auto it = params.find(key);
    return it != params.end() ? it->second : default_value;
}

bool ParamReader::HasFlag(const std::string &flag) const
{
    return std::find(m_flags.begin(), m_flags.end(), flag) != m_flags.end();
}
