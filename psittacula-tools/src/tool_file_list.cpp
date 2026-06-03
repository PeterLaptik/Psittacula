#include "tool_file_list.h"
#include "working_dir.h"
#include "format_util.h"
#include "console_writer.h"
#include <filesystem>
#include <sstream>
#include <algorithm>

void FileListTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "path",
        "string",
        "Directory path to list.",
        true
        });

    params_acc.push_back({
        "recursive",
        "boolean",
        "If true, list files recursively.",
        false
        });

    params_acc.push_back({
        "include_hidden",
        "boolean",
        "If true, include hidden files (starting with '.').",
        false
        });
}

std::string FileListTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("File list tool.", console::TextOrigin::filesystem);

    std::string path = GetParam(params_values, "path");
    bool recursive = GetParamBool(params_values, "recursive", true);
    bool include_hidden = GetParamBool(params_values, "include_hidden", false);

    if (path.empty())
    {
        console::write_line("Missing required parameter: path", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: path"}})";
    }

    console::write_line("Screening: " + path, console::TextOrigin::filesystem);

    if (!wdir.IsInWorkDir(path))
    {
        console::write_line(fmt.Format("Permission_denied: path is outside working directory: %?", path), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"permission_denied\",\"message\":\"Path is outside working directory\",\"path\":\"%?\"}}",
            path
        );
    }

    if (!std::filesystem::exists(path))
    {
        console::write_line(fmt.Format("Directory does not exist: %?", path), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"not_found\",\"message\":\"Directory does not exist\",\"path\":\"%?\"}}",
            path
        );
    }

    if (!std::filesystem::is_directory(path))
    {
        console::write_line(fmt.Format("Path is not a directory: %?", path), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"invalid_arguments\",\"message\":\"Path is not a directory\",\"path\":\"%?\"}}",
            path
        );
    }

    return BuildListingJSON(path, path, recursive, include_hidden);
}

std::string FileListTool::BuildListingJSON(const std::string &rel_path,
    const std::string &abs_path,
    bool recursive,
    bool include_hidden)
{
    Formatter fmt;
    std::ostringstream json;
    json << "{ \"status\": \"success\", \"directory\": \"" << FormatJSONString(rel_path) << "\", \"entries\": [";

    bool first = true;

    auto process_entry = [&](const std::filesystem::directory_entry &entry, const std::string &base_rel) {
        std::string name = entry.path().filename().string();

        if (!include_hidden && !name.empty() && name[0] == '.')
            return;

        if (!first) json << ",";
        first = false;

        bool is_dir = entry.is_directory();
        std::uintmax_t size = 0;

        if (!is_dir)
        {
            try { size = entry.file_size(); }
            catch (...) { size = 0; }
        }

        json << "{"
            << "\"name\":\"" << FormatJSONString(name) << "\","
            << "\"type\":\"" << (is_dir ? "directory" : "file") << "\","
            << "\"size_bytes\":" << size
            << "}";
        };

    if (recursive)
    {
        for (auto &entry : std::filesystem::recursive_directory_iterator(abs_path))
            process_entry(entry, rel_path);
    }
    else
    {
        for (auto &entry : std::filesystem::directory_iterator(abs_path))
            process_entry(entry, rel_path);
    }

    json << "], "
        << "\"message\": \"Directory listing"
        << (recursive ? " (recursive)" : "")
        << " completed\" }";

    return json.str();
}
