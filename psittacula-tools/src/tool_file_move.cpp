#include "tool_file_move.h"
#include "working_dir.h"
#include "format_util.h"
#include "console_writer.h"

#include <filesystem>
#include <iostream>

void FileMoveTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "from",
        "string",
        "Source file path.",
        true
        });

    params_acc.push_back({
        "to",
        "string",
        "Destination file path.",
        true
        });
}

std::string FileMoveTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("File move tool.", console::TextOrigin::filesystem);

    std::string rel_from = GetParam(params_values, "from");
    std::string rel_to = GetParam(params_values, "to");

    if (rel_from.empty() || rel_to.empty())
    {
        console::write_line("Missing required parameters: from/to", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameters: from, to"}})";
    }

    old_path = std::filesystem::path(wdir.GetProjectDir() + rel_from).string();
    new_path = std::filesystem::path(wdir.GetProjectDir() + rel_to).string();

    if (!wdir.IsInWorkDir(old_path) || !wdir.IsInWorkDir(new_path))
    {
        console::write_line("Permission denied: path outside working directory", console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"permission_denied\",\"message\":\"One or both paths are outside working directory\",\"from\":\"%?\",\"to\":\"%?\"}}",
            rel_from, rel_to
        );
    }

    if (!std::filesystem::exists(old_path))
    {
        console::write_line(fmt.Format("Source file does not exist: %?", rel_from), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"not_found\",\"message\":\"Source file does not exist\",\"path\":\"%?\"}}",
            rel_from
        );
    }

    console::write_line(fmt.Format("Moving file: %? -> %?", old_path, new_path), console::TextOrigin::filesystem);

    std::error_code ec;
    std::filesystem::rename(old_path, new_path, ec);

    if (ec)
    {
        console::write_line(fmt.Format("Failed to move file: %?", ec.message()), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"runtime_error\",\"message\":\"Failed to move file: %?\",\"from\":\"%?\",\"to\":\"%?\"}}",
            ec.message(), rel_from, rel_to
        );
    }

    executed = true;

    return fmt.Format(
        "{"
        "\"status\":\"success\","
        "\"file\":{"
        "\"from\":\"%?\","
        "\"to\":\"%?\","
        "\"moved\":true"
        "}"
        "}",
        rel_from, rel_to
    );
}

void FileMoveTool::Undo()
{
    if (!executed)
        return;

    console::write_line("Undo file move: " + new_path + " -> " + old_path, console::TextOrigin::filesystem);

    std::error_code ec;
    std::filesystem::rename(new_path, old_path, ec);
}

void FileMoveTool::Redo()
{
    if (!executed)
        return;

    console::write_line("Redo file move: " + old_path + " -> " + new_path, console::TextOrigin::filesystem);

    std::error_code ec;
    std::filesystem::rename(old_path, new_path, ec);
}
