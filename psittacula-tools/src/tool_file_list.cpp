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

    params_acc.push_back({
        "summary_only",
        "boolean",
        "If true, return only counts instead of full listing.",
        false
        });
}

std::string FileListTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("File list tool.", console::TextOrigin::filesystem);

    std::string path = GetParam(params_values, "path");
    UnescapeSlashesInPath(path);
    console::write_line("Path: " + path, console::TextOrigin::filesystem);

    bool recursive = GetParamBool(params_values, "recursive", false);
    bool include_hidden = GetParamBool(params_values, "include_hidden", false);
    bool summary_only = GetParamBool(params_values, "summary_only", false);

    if (path.empty())
    {
        console::write_line("Missing required parameter: path", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: path"}})";
    }

    // Prevent catastrophic root listings
    if (path == "/" || path == "." || path == "./")
    {
        return R"({"error":{"type":"invalid_arguments","message":"Listing root or current directory is not allowed"}})";
    }

    console::write_line("Screening: " + path, console::TextOrigin::filesystem);

    if (!wdir.IsInWorkDir(path))
    {
        console::write_line(fmt.Format("Permission_denied: path is outside working directory: %?", path),
            console::TextOrigin::error);
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

    return BuildListingJSON(path, recursive, include_hidden, summary_only);
}

std::string FileListTool::BuildListingJSON(const std::string &input_path,
    bool recursive,
    bool include_hidden,
    bool summary_only) const
{
    Formatter fmt;
    std::ostringstream json;

    std::filesystem::path abs_path = std::filesystem::absolute(input_path);

    size_t count_files = 0;
    size_t count_dirs = 0;
    size_t total_entries = 0;

    json << "{ \"status\": \"success\", "
        << "\"directory\": \"" << GetEscapedJSONString(input_path) << "\", ";

    if (summary_only)
    {
        for (const auto &entry : std::filesystem::directory_iterator(abs_path))
        {
            std::string filename = entry.path().filename().string();
            if (!include_hidden && !filename.empty() && filename[0] == '.')
                continue;

            total_entries++;
            if (entry.is_directory()) count_dirs++;
            else count_files++;
        }

        json << "\"summary\": {"
            << "\"total_entries\": " << total_entries << ","
            << "\"directories\": " << count_dirs << ","
            << "\"files\": " << count_files
            << "}, \"complete\": true }";

        return json.str();
    }

    json << "\"entries\": [";

    bool first = true;
    size_t emitted = 0;

    auto process_entry = [&](const std::filesystem::directory_entry &entry)
        {
            if (emitted >= MAX_ENTRIES)
                return;

            const auto &p = entry.path();
            std::string filename = p.filename().string();

            if (!include_hidden && !filename.empty() && filename[0] == '.')
                return;

            bool is_dir = entry.is_directory();
            std::uintmax_t size = 0;

            if (!is_dir)
            {
                try { size = entry.file_size(); }
                catch (...) { size = 0; }
            }

            std::string rel_name;
            try { rel_name = std::filesystem::relative(p, abs_path).string(); }
            catch (...) { rel_name = filename; }

            if (!first)
                json << ",";
            first = false;

            json << "{"
                << "\"name\":\"" << GetEscapedJSONString(rel_name) << "\","
                << "\"type\":\"" << (is_dir ? "directory" : "file") << "\","
                << "\"size_bytes\":" << size
                << "}";

            emitted++;

            if (json.tellp() > MAX_JSON_SIZE)
                return;
        };

    if (recursive)
    {
        std::filesystem::recursive_directory_iterator it(abs_path), end;
        for (; it != end; ++it)
        {
            const auto &entry = *it;
            std::string filename = entry.path().filename().string();

            if (!include_hidden && entry.is_directory() &&
                !filename.empty() && filename[0] == '.')
            {
                it.disable_recursion_pending();
                continue;
            }

            if (it.depth() > MAX_DEPTH)
            {
                it.disable_recursion_pending();
                continue;
            }

            process_entry(entry);

            if (emitted >= MAX_ENTRIES || json.tellp() > MAX_JSON_SIZE)
                break;
        }
    }
    else
    {
        for (const auto &entry : std::filesystem::directory_iterator(abs_path))
        {
            process_entry(entry);
            if (emitted >= MAX_ENTRIES || json.tellp() > MAX_JSON_SIZE)
                break;
        }
    }

    json << "], "
        << "\"message\": \"Directory listing"
        << (recursive ? " (recursive)" : "")
        << " completed\", "
        << "\"complete\": true }";

    return json.str();
}