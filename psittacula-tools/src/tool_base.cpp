#include "tool_base.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

std::string ToolBase::GetParam(const std::vector<ToolParameter> &params_acc, const std::string &param_name, std::string default_value)
{
    auto it = std::find_if(params_acc.begin(), params_acc.end(),
        [&param_name](const ToolParameter &tp) {
            return tp.name == param_name;
        });

    return it != params_acc.end() ? it->value : default_value;
}

bool ToolBase::GetParamBool(const std::vector<ToolParameter> &params_acc, const std::string &param_name, bool default_value)
{
    auto it = std::find_if(params_acc.begin(), params_acc.end(),
        [&param_name](const ToolParameter &tp) {
            return tp.name == param_name;
        });

    if (it == params_acc.end())
        return default_value;

    std::string v = it->value;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);

    if (v == "true" || v == "1" || v == "yes" || v == "on")
        return true;

    if (v == "false" || v == "0" || v == "no" || v == "off")
        return false;

    return default_value;
}

std::string ToolBase::FormatJSONString(const std::string &str)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');

    for (unsigned char c : str)
    {
        switch (c)
        {
            case '\"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b";  break;
            case '\f': out << "\\f";  break;
            case '\n': out << "\\n";  break;
            case '\r': out << "\\r";  break;
            case '\t': out << "\\t";  break;

            default:
                if (c < 0x20)
                {
                    out << "\\u" << std::setw(4) << (int)c;
                }
                else
                {
                    out << c;
                }
        }
    }

    return out.str();
}
