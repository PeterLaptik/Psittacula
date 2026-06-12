#include "tool_file_search.h"
#include "console_writer.h"
#include "format_util.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>

void FileSearchTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "query",
        "string",
        "Query.",
        false
        });

    params_acc.push_back({
        "path",
        "string",
        "Path",
        true
        });

    params_acc.push_back({
        "case_sensitive",
        "boolean",
        "If true, it is case sensitive",
        true
        });

    params_acc.push_back({
        "regex",
        "string",
        "Regex",
        true
        });
}

std::string FileSearchTool::Execute(std::vector<ToolParameter> &params_values)
{
    std::string query = GetParam(params_values, "query");
    std::string path = GetParam(params_values, "path");
    bool case_sensitive = GetParamBool(params_values, "case_sensitive", false);
    bool use_regex = GetParamBool(params_values, "regex", false);

    if (path.empty())
    {
        console::write_line("Missing required parameter: path", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: path"}})";
    }

    if (query.empty())
    {
        console::write_line("Missing required field: query", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: path"}})";
    }

    auto matches = SearchDirectory(path, query, case_sensitive, use_regex);

    Formatter fmt;
    std::string result;
    for (int i = 0, max_sz = matches.size(); i < max_sz; i++)
    {
        auto &m = matches[i];
        result += fmt.Format("{\"file\": \"%?\",\"line\": %?,\"snippet\":\"%?\"}", m.file, m.line, m.snippet);
        if (i != max_sz - 1)
            result += ",";
    }

    std::string response = fmt.Format(
        "{"
        "\"success\": true,"
        "\"result\": [%?],"
        "\"message\": \"Found %? match%?\""
        "}",
        result,
        matches.size(),
        matches.size() == 1 ? "" : "es"
    );
    return response;
}

std::vector<FileSearchTool::Match> FileSearchTool::SearchDirectory(
    const std::string &root,
    const std::string &query,
    bool case_sensitive,
    bool use_regex)
{
    std::vector<Match> results;

    for (auto &entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file())
            continue;

        auto file_matches = SearchFile(entry.path().string(), query, case_sensitive, use_regex);
        results.insert(results.end(), file_matches.begin(), file_matches.end());
    }

    return results;
}

std::vector<FileSearchTool::Match> FileSearchTool::SearchFile(
    const std::string &path,
    const std::string &query,
    bool case_sensitive,
    bool use_regex)
{
    std::vector<Match> results;
    std::ifstream file(path);
    if (!file.is_open())
        return results;

    std::string line;
    int line_number = 0;

    std::regex re;
    if (use_regex)
    {
        auto flags = case_sensitive ? std::regex::ECMAScript
            : (std::regex::ECMAScript | std::regex::icase);
        re = std::regex(query, flags);
    }

    while (std::getline(file, line))
    {
        line_number++;

        bool match = false;

        if (use_regex)
        {
            match = std::regex_search(line, re);
        }
        else
        {
            std::string hay = line;
            std::string needle = query;

            if (!case_sensitive)
            {
                std::transform(hay.begin(), hay.end(), hay.begin(), ::tolower);
                std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
            }

            match = hay.find(needle) != std::string::npos;
        }

        if (match)
        {
            results.push_back({ path, line_number, line });
        }
    }

    return results;
}
