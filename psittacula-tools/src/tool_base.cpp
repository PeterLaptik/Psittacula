#include "tool_base.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

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

std::string ToolBase::GetEscapedJSONString(const std::string &str) const
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.String(str.c_str());
    return std::string(buffer.GetString());
}
