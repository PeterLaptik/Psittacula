#include "tool_dir_create.h"
#include "working_dir.h"
#include "format_util.h"
#include "console_writer.h"
#include <filesystem>
#include <iostream>

void DirCreateTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "path",
        "string",
        "Full path of the directory to create.",
        true
        });
}

std::string DirCreateTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("Directory create tool.", console::TextOrigin::filesystem);

    std::string path = GetParam(params_values, "path");
    UnescapeSlashesInPath(path);

    if (path.empty())
    {
        console::write_line("Missing required parameter: path", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: path"}})";
    }

    if (!wdir.IsInWorkDir(path)) 
    {
        console::write_line(fmt.Format("Permission_denied: path is outside working directory: %?", path), console::TextOrigin::error);
        return fmt.Format("{\"error\":{\"type\":\"permission_denied\",\"message\":\"Path is outside working directory\",\"path\":\"%?\"}}", path);
    }

    bool existed_before = std::filesystem::exists(path);

    console::write_line(fmt.Format("Creating directory: %?", path), console::TextOrigin::filesystem);

    if (!std::filesystem::create_directories(path) && !existed_before) 
    {
        console::write_line(fmt.Format("Failed to create directory: %?", path), console::TextOrigin::error);
        return fmt.Format("{\"error\":{\"type\":\"runtime_error\",\"message\":\"Failed to create directory\",\"path\":\"%?\"}}", path);
    }

    last_path = path;
    executed = true;
    dir_existed_before = existed_before;

    return fmt.Format(
        "{\"status\":\"success\","          
        "\"directory\":{"
        "\"path\":\"%?\","
        "\"created\":true,"
        "\"existed_before\":%?"
        "},"
        "\"message\":\"Directory %?\"}",
        path,
        (existed_before ? "true" : "false"),
        (existed_before ? "already existed" : "created successfully")
    );
}

void DirCreateTool::Undo()
{
    if (!executed)
        return;

    // If the directory existed before creation, do nothing
    if (dir_existed_before)
        return;

    std::cout << "Removing directory: " << last_path << std::endl;

    // Check if directory is empty
    bool is_empty = std::filesystem::is_empty(last_path);

    if (!is_empty) 
    {
        console::write_line("Undo skipped: directory is not empty: " + last_path, console::TextOrigin::error);
        return;
    }

    // Safe to remove
    std::error_code ec;
    std::filesystem::remove(last_path, ec);

    if (ec) 
    {
        console::write_line("Failed to remove directory: " + ec.message(), console::TextOrigin::error);
    }
}

void DirCreateTool::Redo()
{
    if (!executed)
        return;

    if (dir_existed_before) 
    {
        // Directory existed before: nothing to redo
        return;
    }

    std::cout << "Recreating sirectory: " << last_path << std::endl;

    std::filesystem::create_directories(last_path);
}