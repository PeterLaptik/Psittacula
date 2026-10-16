#include "tool_base.h"
#include "console_writer.h"
#include "utf8_util.h"
#include "working_dir.h"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

std::string ToolBase::ResolveProjectPath(const std::string &input)
{
    namespace fs = std::filesystem;

    const fs::path requested = input;

    if (requested.is_absolute())
        return requested.string();

    return (fs::path(WorkingDir::GetInstance().GetProjectDir()) / requested).string();
}


std::string ToolBase::GetParam(const std::vector<ToolParameter> &params_acc, const std::string &param_name, std::string default_value) const
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
    // Invalid UTF-8 bytes must never reach the request body: the server side
    // rejects the whole request with a JSON parse error
    const std::string safe = utf8::Sanitize(str);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.String(safe.c_str(), static_cast<rapidjson::SizeType>(safe.size()));
    return std::string(buffer.GetString(), buffer.GetSize());
}

void ToolBase::UnEscapeSlashesInPath(std::string &value) const
{
    std::string from = "\\\\";
    std::string to = "\\";

    size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.length(), to);
        pos += to.length();
    }
}
