#include "tool_dir_delete.h"
#include "working_dir.h"
#include "format_util.h"
#include "console_writer.h"

#include <filesystem>
#include <iostream>

void DirDeleteTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "path",
        "string",
        "Full path of the directory to delete.",
        true
        });

    params_acc.push_back({
        "recursive",
        "boolean",
        "If true, delete directory recursively.",
        false
        });
}

std::string DirDeleteTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("Directory delete tool.", console::TextOrigin::filesystem);

    std::string rel_path = GetParam(params_values, "path");
    recursive = GetParamBool(params_values, "recursive", false);

    if (rel_path.empty())
    {
        console::write_line("Missing required parameter: path", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: path"}})";
    }

    std::string path = wdir.GetProjectDir() + rel_path;

    if (!wdir.IsInWorkDir(path))
    {
        console::write_line(fmt.Format("Permission denied: %?", path), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"permission_denied\",\"message\":\"Path is outside working directory\",\"path\":\"%?\"}}",
            rel_path
        );
    }

    if (!std::filesystem::exists(path))
    {
        console::write_line(fmt.Format("Directory does not exist: %?", path), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"not_found\",\"message\":\"Directory does not exist\",\"path\":\"%?\"}}",
            rel_path
        );
    }

    if (!std::filesystem::is_directory(path))
    {
        console::write_line(fmt.Format("Path is not a directory: %?", path), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"invalid_arguments\",\"message\":\"Path is not a directory\",\"path\":\"%?\"}}",
            rel_path
        );
    }

    if (!std::filesystem::is_empty(path))
    {
        console::write_line(fmt.Format("Directory is not empty: %?", path), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"runtime_error\",\"message\":\"Directory is not empty\",\"path\":\"%?\"}}",
            rel_path
        );
    }

    existed_before = true;
    last_path = path;

    console::write_line(fmt.Format("Deleting directory: %?", path), console::TextOrigin::filesystem);

    std::error_code ec;

    if (recursive)
    {
        std::filesystem::remove_all(path, ec);
    }
    else
    {
        if (!std::filesystem::is_empty(path))
        {
            console::write_line("Directory is not empty; recursive=false", console::TextOrigin::error);
            return fmt.Format(
                "{\"error\":{\"type\":\"runtime_error\",\"message\":\"Directory is not empty\",\"path\":\"%?\"}}",
                rel_path
            );
        }

        std::filesystem::remove(path, ec);
    }

    if (ec)
    {
        console::write_line(fmt.Format("Failed to delete directory: %?", ec.message()), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"runtime_error\",\"message\":\"Failed to delete directory: %?\",\"path\":\"%?\"}}",
            ec.message(), rel_path
        );
    }

    executed = true;

    return fmt.Format(
        "{"
        "\"status\":\"success\","
        "\"directory\":{"
        "\"path\":\"%?\","
        "\"deleted\":true,"
        "\"recursive\":%?"
        "},"
        "\"message\":\"Directory deleted%?\""
        "}",
        rel_path,
        recursive ? "true" : "false",
        recursive ? " recursively" : ""
    );
}

void DirDeleteTool::Undo()
{
    if (!executed)
        return;

    console::write_line("Undo directory delete: " + last_path, console::TextOrigin::filesystem);

    // We cannot restore deleted directory contents unless we snapshot them.
    // For safety, we only recreate the directory itself.
    std::filesystem::create_directories(last_path);
}

void DirDeleteTool::Redo()
{
    if (!executed)
        return;

    console::write_line("Redo directory delete: " + last_path, console::TextOrigin::filesystem);

    std::error_code ec;

    if (recursive)
        std::filesystem::remove_all(last_path, ec);
    else
        std::filesystem::remove(last_path, ec);
}
