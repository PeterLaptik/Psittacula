#ifndef PARAM_READER_INCLUDED_H
#define PARAM_READER_INCLUDED_H

#include <string>
#include <vector>
#include <map>

class ParamReader
{
    public:
        ParamReader(char **argv, int argc);
        ~ParamReader() = default;

        void ReadParams();

        std::string GetParam(const std::string &key, const std::string &default_value = "") const;

        bool HasFlag(const std::string &flag) const;

    private:
        std::map<std::string, std::string> params;
        std::vector<std::string> m_flags;
        char **m_argv;
        int m_argc;
};

#endif // PARAM_READER_INCLUDED_H