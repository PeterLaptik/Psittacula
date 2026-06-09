#include "tool_file_create.h"
#include "working_dir.h"
#include "format_util.h"
#include "console_writer.h"
#include <fstream>
#include <filesystem>
#include <iostream>

void FileCreateTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "path",
        "string",
        "Full path of the file to create.",
        true
        });

    params_acc.push_back({
        "content",
        "string",
        "Content to write into the file.",
        false
        });
}

std::string FileCreateTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("File create tool.", console::TextOrigin::filesystem);

    std::string path = GetParam(params_values, "path");
    std::string content = GetParam(params_values, "content");

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

    console::write_line("Creating file: " + path, console::TextOrigin::filesystem);

    last_path = path;
    last_content = content;
    executed = false;
    existing_file_dump.clear();

    // Create parent dird if do not exist
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    bool file_exists = std::filesystem::exists(path);

    // If overwriting, save old content
    if (file_exists) 
    {
        std::ifstream in(path);
        if (in) 
        {
            existing_file_dump.assign(
                std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>()
            );
        }
        in.close();
    }

    // Write new content
    std::ofstream out(path);
    if (!out) 
    {
        console::write_line(fmt.Format("Failed to create file: %?", path), console::TextOrigin::error);
        return fmt.Format("{\"error\":{\"type\":\"runtime_error\",\"message\":\"Failed to create file\",\"path\":\"%?\"}}", path);
    }

    out << content;
    out.close();

    executed = true;

    // Compute file size
    std::uintmax_t size = 0;
    try 
    { 
        size = std::filesystem::file_size(path); 
    }
    catch (...) 
    { 
        size = content.size(); 
    }

    // Build JSON response
    std::string result_template =
        "{"
        "  \"status\": \"success\","
        "  \"file\": {"
        "    \"path\": \"%?\","
        "    \"size_bytes\": %?,"
        "    \"created\": true,"
        "    \"overwritten\": %?"
        "  },"
        "  \"message\": \"File %?\""
        "}";

    return fmt.Format(
        result_template,
        path,
        size,
        file_exists ? "true" : "false",
        file_exists ? "overwritten" : "created"
    );
}

void FileCreateTool::Undo()
{
    if (!executed)
        return;

    console::write_line("Removing file: " + last_path, console::TextOrigin::filesystem);

    if (!existing_file_dump.empty()) 
    {
        // Restore previous content
        std::ofstream out(last_path);
        out << existing_file_dump;
        out.close();
    }
    else {
        // File did not exist before: remove it
        if (std::filesystem::exists(last_path)) 
        {
            std::filesystem::remove(last_path);
        }
    }
}

void FileCreateTool::Redo()
{
    if (!executed)
        return;

    console::write_line("Creating file again: " + last_path, console::TextOrigin::filesystem);

    std::ofstream out(last_path);
    out << last_content;
    out.close();
}