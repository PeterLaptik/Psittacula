#include "tool_file_modify.h"
#include "working_dir.h"
#include "format_util.h"
#include "console_writer.h"
#include <fstream>
#include <filesystem>
#include <sstream>

void FileModifyTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({ "path", "string", "Path of the file to modify.", true });
    params_acc.push_back({ "content", "string", "Content to write.", true });

    params_acc.push_back({
        "mode",
        "string",
        "One of: overwrite, insert, append, replace_range.",
        true
        });

    params_acc.push_back({
        "position",
        "integer",
        "Used only when mode = insert.",
        false,
        "0"
        });

    params_acc.push_back({
        "range_start",
        "integer",
        "Used only when mode = replace_range.",
        false,
        "0"
        });

    params_acc.push_back({
        "range_end",
        "integer",
        "Used only when mode = replace_range.",
        false,
        "0"
        });

    params_acc.push_back({
        "reason",
        "string",
        "Explanation of why modification is needed.",
        false
        });

    params_acc.push_back({
        "diff_required",
        "boolean",
        "If true, model must request file content before modifying.",
        false,
        "true"
        });
}

std::string FileModifyTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    std::string path = GetParam(params_values, "path");
    UnescapeSlashesInPath(path);

    std::string content = GetParam(params_values, "content");
    mode = GetParam(params_values, "mode");

    if (path.empty() || mode.empty())
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameters"}})";

    if (!wdir.IsInWorkDir(path))
        return fmt.Format(
            "{\"error\":{\"type\":\"permission_denied\",\"message\":\"Path outside working directory\",\"path\":\"%?\"}}",
            path
        );

    // Optional parameters
    position = static_cast<size_t>(std::stoll(GetParam(params_values, "position", "0")));
    range_start = static_cast<size_t>(std::stoll(GetParam(params_values, "range_start", "0")));
    range_end = static_cast<size_t>(std::stoll(GetParam(params_values, "range_end", "0")));

    // Read old content
    if (std::filesystem::exists(path)) {
        std::ifstream in(path);
        std::stringstream buffer;
        buffer << in.rdbuf();
        old_content = buffer.str();
    }
    else {
        old_content.clear();
    }

    // Compute new content based on mode
    if (mode == "overwrite") {
        new_content = content;
    }
    else if (mode == "append") {
        new_content = old_content + content;
    }
    else if (mode == "insert") {
        if (position > old_content.size())
            position = old_content.size();

        new_content =
            old_content.substr(0, position) +
            content +
            old_content.substr(position);
    }
    else if (mode == "replace_range") {
        if (range_start > old_content.size())
            range_start = old_content.size();
        if (range_end > old_content.size())
            range_end = old_content.size();
        if (range_start > range_end)
            std::swap(range_start, range_end);

        new_content =
            old_content.substr(0, range_start) +
            content +
            old_content.substr(range_end);
    }
    else {
        return R"({"error":{"type":"invalid_arguments","message":"Invalid mode"}})";
    }

    // Write new content
    std::ofstream out(path);
    if (!out)
        return fmt.Format(
            "{\"error\":{\"type\":\"runtime_error\",\"message\":\"Failed to write file\",\"path\":\"%?\"}}",
            path
        );

    out << new_content;
    out.close();

    last_path = path;
    executed = true;

    return fmt.Format(
        "{"
        "\"status\":\"success\","
        "\"file_modified\":{"
        "\"path\":\"%?\","
        "\"old_size\":%?,"
        "\"new_size\":%?,"
        "\"mode\":\"%?\","
        "\"position\":%?,"
        "\"range_start\":%?,"
        "\"range_end\":%?"
        "}"
        "}",
        path,
        old_content.size(),
        new_content.size(),
        mode,
        position,
        range_start,
        range_end
    );
}

void FileModifyTool::Undo()
{
    if (!executed)
        return;

    std::ofstream out(last_path);
    if (out) {
        out << old_content;
        out.close();
    }
}

void FileModifyTool::Redo()
{
    if (!executed)
        return;

    std::ofstream out(last_path);
    if (out) {
        out << new_content;
        out.close();
    }
}
