#include "tool_file_remove.h"
#include "working_dir.h"
#include "format_util.h"
#include "console_writer.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>

void FileRemoveTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "path",
        "string",
        "Full path of the file to remove.",
        true
        });
}

std::string FileRemoveTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("File remove tool.", console::TextOrigin::filesystem);

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

    console::write_line("Removing file: " + path, console::TextOrigin::filesystem);

    if (!std::filesystem::exists(path)) 
    {
        console::write_line(fmt.Format("File does not exist: %?", path), console::TextOrigin::error);
        return fmt.Format("{\"error\":{\"type\":\"not_found\",\"message\":\"File does not exist\",\"path\":\"%?\"}}", path);
    }

    // Save file content for Undo()
    std::ifstream in(path);
    if (in) 
    {
        std::stringstream buffer;
        buffer << in.rdbuf();
        last_content = buffer.str();
    }
    in.close();

    last_path = path;

    // Remove file
    std::filesystem::remove(path);
    executed = true;

    // Success JSON
    return fmt.Format(
        "{"
        "\"status\":\"success\","
        "\"file_removed\":{"
        "\"path\":\"%?\""
        "},"
        "\"message\":\"File removed\""
        "}",
        path
    );
}

void FileRemoveTool::Undo()
{
    if (!executed)
        return;

    console::write_line("Restoring file: " + last_path, console::TextOrigin::filesystem);

    std::ofstream out(last_path);
    if (out) 
    {
        out << last_content;
        out.close();
    }
    else 
    {
        console::write_line("Failed to restore file: " + last_path, console::TextOrigin::error);
    }
}

void FileRemoveTool::Redo()
{
    if (!executed)
        return;

    console::write_line("Removing file again: " + last_path, console::TextOrigin::filesystem);

    if (std::filesystem::exists(last_path)) 
    {
        std::filesystem::remove(last_path);
    }
}