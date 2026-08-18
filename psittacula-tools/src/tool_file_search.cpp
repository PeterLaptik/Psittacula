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
        "Plain-text search query.",
        false
        });

    params_acc.push_back({
        "regex",
        "string",
        "Regex pattern to match.",
        false
        });

    params_acc.push_back({
        "regex_mode",
        "boolean",
        "If true, use regex pattern instead of plain-text search.",
        false
        });

    params_acc.push_back({
        "path",
        "string",
        "Directory path to search.",
        true
        });

    params_acc.push_back({
        "case_sensitive",
        "boolean",
        "If true, search is case-sensitive.",
        false
        });
}

std::string FileSearchTool::Execute(std::vector<ToolParameter> &params_values)
{
    console::write_line("File search tool.", console::TextOrigin::filesystem);

    std::string path = GetParam(params_values, "path");
    console::write_line("Search path:", console::TextOrigin::filesystem);
    UnescapeSlashesInPath(path);

    bool case_sensitive = GetParamBool(params_values, "case_sensitive", false);
    bool regex_mode = GetParamBool(params_values, "regex_mode", false);

    std::string query = GetParam(params_values, "query");
    std::string regex_pattern = GetParam(params_values, "regex");
    console::write_line(query.empty() ? ("Pattern: " + regex_pattern) : ("Query: " + query), console::TextOrigin::filesystem);

    if (path.empty())
    {
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: path"}})";
    }

    if (regex_mode)
    {
        if (regex_pattern.empty())
        {
            return R"({"error":{"type":"invalid_arguments","message":"regex_mode=true but no regex pattern provided"}})";
        }
    }
    else
    {
        if (query.empty())
        {
            return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: query"}})";
        }
    }

    auto matches = SearchDirectory(
        path,
        regex_mode ? regex_pattern : query,
        case_sensitive,
        regex_mode
    );

    Formatter fmt;
    std::string result;

    for (int i = 0; i < matches.size(); i++)
    {
        auto &m = matches[i];
        result += fmt.Format(
            "{\"file\":\"%?\",\"line\":%?,\"snippet\":\"%?\"}",
            m.file, m.line, m.snippet
        );
        if (i + 1 < matches.size())
            result += ",";
    }

    return fmt.Format(
        "{"
        "\"success\":true,"
        "\"result\":[%?],"
        "\"message\":\"Found %? match%?\""
        "}",
        result,
        matches.size(),
        matches.size() == 1 ? "" : "es"
    );
}

std::vector<FileSearchTool::Match> FileSearchTool::SearchDirectory(
    const std::string &root,
    const std::string &pattern,
    bool case_sensitive,
    bool use_regex)
{
    std::vector<Match> results;

    for (auto &entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file())
            continue;

        auto file_matches = SearchFile(
            entry.path().string(),
            pattern,
            case_sensitive,
            use_regex
        );

        results.insert(results.end(), file_matches.begin(), file_matches.end());
    }

    return results;
}

std::vector<FileSearchTool::Match> FileSearchTool::SearchFile(
    const std::string &path,
    const std::string &pattern,
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
        re = std::regex(pattern, flags);
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
            std::string needle = pattern;

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
