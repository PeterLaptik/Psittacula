#include "tool_file_content_modify.h"
#include "tool_base.h"
#include "format_util.h"
#include "console_writer.h"
#include <fstream>
#include <sstream>
#include <filesystem>

void FileContentModifyTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "file_path",
        "string",
        "The absolute path to the file being modified.",
        true
    });
    
    params_acc.push_back({
        "old_str",
        "string",
        "The exact target string of code or text to be replaced. Must appear exactly once in the file.",
        true
    });
    
    params_acc.push_back({
        "new_str",
        "string",
        "The new content to replace the old_str with.",
        true
    });
}

std::string FileContentModifyTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("File content modify tool.", console::TextOrigin::filesystem);

    std::string file_path = GetParam(params_values, "file_path");
    UnescapeSlashesInPath(file_path);

    std::string old_str = GetParam(params_values, "old_str");
    std::string new_str = GetParam(params_values, "new_str");

    if (file_path.empty())
    {
        console::write_line("Missing required parameter: file_path", console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"invalid_arguments\",\"message\":\"Missing required parameter: file_path\"}}"
        );
    }

    if (old_str.empty())
    {
        console::write_line("Missing required parameter: old_str", console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"invalid_arguments\",\"message\":\"Missing required parameter: old_str\"}}"
        );
    }

    if (new_str.empty())
    {
        console::write_line("Missing required parameter: new_str", console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"invalid_arguments\",\"message\":\"Missing required parameter: new_str\"}}"
        );
    }

    if (!wdir.IsInWorkDir(file_path)) 
    {
        console::write_line(fmt.Format("Permission_denied: path is outside working directory: %?", file_path), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"permission_denied\",\"message\":\"Path is outside working directory\",\"path\":\"%?\"}}",
            file_path
        );
    }

    // Check if file exists and read its content
    if (!std::filesystem::exists(file_path)) 
    {
        console::write_line("File does not exist: " + file_path, console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"runtime_error\",\"message\":\"File does not exist\",\"path\":\"%?\"}}",
            file_path
        );
    }

    // Read old content
    std::ifstream in(file_path);
    if (!in) 
    {
        console::write_line("Failed to read file: " + file_path, console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"runtime_error\",\"message\":\"Failed to read file\",\"path\":\"%?\"}}",
            file_path
        );
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    old_content = buffer.str();
    in.close();

    // Check if old_str appears exactly once in the file
    size_t count = old_content.find(old_str);
    while (count != std::string::npos) 
    {
        count += old_content.find(old_str, count + old_str.length());
        count++;
    }

    // Count occurrences
    size_t occurrence_count = 0;
    size_t pos = 0;
    while ((pos = old_content.find(old_str, pos)) != std::string::npos)
    {
        occurrence_count++;
        pos += old_str.length() + 1;
    }

    if (occurrence_count != 1) 
    {
        console::write_line("Error: old_str must appear exactly once in the file (found " + std::to_string(occurrence_count) + " occurrences)", console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"invalid_arguments\",\"message\":\"old_str must appear exactly once in the file\",\"occurrences\":%?}}",
            occurrence_count
        );
    }

    // Replace old_str with new_str
    std::string result = old_content;
    size_t first_pos = result.find(old_str);
    result.replace(first_pos, old_str.length(), new_str);

    // Write new content
    std::ofstream out(file_path);
    if (!out) 
    {
        console::write_line(fmt.Format("Failed to write file: %?", file_path), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"runtime_error\",\"message\":\"Failed to write file\",\"path\":\"%?\"}}",
            file_path
        );
    }

    out << result;
    out.close();

    last_path = file_path;
    executed = true;

    // Success JSON
    return fmt.Format(
        "{\"\n"
        "  \"status\": \"success\",\n"
        "  \"file_modified\": {\n"
        "    \"path\": \"%?\",\n"
        "    \"old_size\": %?,\n"
        "    \"new_size\": %?,\n"
        "    \"old_str\": \"%?\",\n"
        "    \"new_str\": \"%?\"\n"
        "  },\n"
        "  \"message\": \"File surgically modified\"\n"
        "}"
    , file_path, old_content.size(), result.size(), old_str, new_str);
}

void FileContentModifyTool::Undo()
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

void FileContentModifyTool::Redo()
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