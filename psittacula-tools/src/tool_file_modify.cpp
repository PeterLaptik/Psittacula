#include "tool_file_modify.h"
#include "working_dir.h"
#include "format_util.h"
#include "console_writer.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>

void FileModifyTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "path",
        "string",
        "Full path of the file to modify.",
        true
        });
    
    params_acc.push_back({
        "content",
        "string",
        "Content to insert or replace in the file.",
        true
        });
    
    params_acc.push_back({
        "position",
        "integer",
        "Position to insert content at (0 = beginning, -1 = append, default = -1).",
        false
        });
    
    params_acc.push_back({
        "replace_all",
        "boolean",
        "If true, replaces entire file content; if false, inserts at position (default = false).",
        false
        });
}

std::string FileModifyTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("File modify tool.", console::TextOrigin::filesystem);

    std::string rel_path = GetParam(params_values, "path");
    std::string content = GetParam(params_values, "content");

    if (rel_path.empty()) 
    {
        console::write_line("Missing required parameter: path", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: path"}})";
    }

    std::string path = wdir.GetProjectDir() + rel_path;

    if (!wdir.IsInWorkDir(path)) 
    {
        console::write_line(fmt.Format("Permission_denied: path is outside working directory: %?", path), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"permission_denied\",\"message\":\"Path is outside working directory\",\"path\":\"%?\"}}",
            rel_path
        );
    }

    // Parse optional parameters
    int pos_int = -1;
    bool replace_all_bool = false;

    for (auto &p : params_values) 
    {
        if (p.name == "position") 
        {
            try { 
                pos_int = std::stoi(p.value); 
            }
            catch (...) 
            { 
                pos_int = -1; 
            }
        }
        if (p.name == "replace_all") 
        {
            replace_all_bool = (p.value == "true" || p.value == "1");
        }
    }

    position = static_cast<size_t>(pos_int);
    replace_all = replace_all_bool;

    console::write_line("Modifying file: " + path, console::TextOrigin::filesystem);

    // Read old content if file exists
    if (std::filesystem::exists(path)) 
    {
        std::ifstream in(path);
        if (in) 
        {
            std::stringstream buffer;
            buffer << in.rdbuf();
            old_content = buffer.str();
        }
        in.close();
    }
    else 
    {
        old_content = "";
    }

    // Compute new content
    if (replace_all || old_content.empty()) 
    {
        new_content = content;
    }
    else 
    {
        if (position == static_cast<size_t>(-1) || position >= old_content.length()) 
        {
            new_content = old_content + content;  // append
        }
        else 
        {
            new_content = old_content.substr(0, position) +
                content +
                old_content.substr(position);
        }
    }

    // Write new content
    std::ofstream out(path);
    if (!out) 
    {
        console::write_line(fmt.Format("Failed to write file: %?", path), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"runtime_error\",\"message\":\"Failed to write file\",\"path\":\"%?\"}}",
            path
        );
    }

    out << new_content;
    out.close();

    last_path = path;
    executed = true;

    // Success JSON
    std::string result_template =
        "{"
        "  \"status\": \"success\","
        "  \"file_modified\": {"
        "    \"path\": \"%?\","
        "    \"old_size\": %?,"
        "    \"new_size\": %?,"
        "    \"replace_all\": %?,"
        "    \"position\": %?"
        "  },"
        "  \"message\": \"File %?\""
        "}";

    return fmt.Format(
        result_template,
        path,
        old_content.size(),
        new_content.size(),
        replace_all ? "true" : "false",
        pos_int,
        replace_all ? "fully replaced" : "modified"
    );
}

void FileModifyTool::Undo()
{
    if (!executed)
        return;

    console::write_line("Restoring file: " + last_path, console::TextOrigin::filesystem);

    std::ofstream out(last_path);
    if (out) 
    {
        out << old_content;
        out.close();
    }
    else 
    {
        console::write_line("Failed to restore file: " + last_path, console::TextOrigin::error);
    }
}

void FileModifyTool::Redo()
{
    if (!executed)
        return;

    console::write_line("Reapplying modification: " + last_path, console::TextOrigin::filesystem);

    std::ofstream out(last_path);
    if (out) 
    {
        out << new_content;
        out.close();
    }
    else 
    {
        console::write_line("Failed to reapply modification to file: " + last_path, console::TextOrigin::error);
    }
}