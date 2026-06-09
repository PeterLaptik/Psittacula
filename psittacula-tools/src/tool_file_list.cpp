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

    return BuildListingJSON(path, recursive, include_hidden);
}

std::string FileListTool::BuildListingJSON(const std::string &input_path,
    bool recursive,
    bool include_hidden)
{
    Formatter fmt;
    std::ostringstream json;

    // Normalize to absolute path for filesystem traversal
    std::filesystem::path abs_path = std::filesystem::absolute(input_path);

    json << "{ \"status\": \"success\", "
        << "\"directory\": \"" << GetEscapedJSONString(input_path) << "\", "
        << "\"entries\": [";

    bool first = true;

    auto process_entry = [&](const std::filesystem::directory_entry &entry)
        {
            const auto &p = entry.path();
            std::string filename = p.filename().string();

            // Hidden file/dir filtering
            if (!include_hidden && !filename.empty() && filename[0] == '.')
                return;

            if (!first)
                json << ",";
            first = false;

            bool is_dir = entry.is_directory();
            std::uintmax_t size = 0;

            if (!is_dir)
            {
                try { size = entry.file_size(); }
                catch (...) { size = 0; }
            }

            // Prefer relative path for output
            std::string rel_name;
            try
            {
                rel_name = std::filesystem::relative(p, abs_path).string();
            }
            catch (...)
            {
                rel_name = filename;
            }

            json << "{"
                << "\"name\":\"" << GetEscapedJSONString(rel_name) << "\","
                << "\"type\":\"" << (is_dir ? "directory" : "file") << "\","
                << "\"size_bytes\":" << size
                << "}";
        };

    if (recursive)
    {
        std::filesystem::recursive_directory_iterator it(abs_path), end;
        for (; it != end; ++it)
        {
            const auto &entry = *it;
            std::string filename = entry.path().filename().string();

            // Prevent recursion into hidden directories
            if (!include_hidden && entry.is_directory() &&
                !filename.empty() && filename[0] == '.')
            {
                it.disable_recursion_pending();
                continue;
            }

            process_entry(entry);
        }
    }
    else
    {
        for (const auto &entry : std::filesystem::directory_iterator(abs_path))
            process_entry(entry);
    }

    json << "], "
        << "\"message\": \"Directory listing"
        << (recursive ? " (recursive)" : "")
        << " completed\" }";

    return json.str();
}
