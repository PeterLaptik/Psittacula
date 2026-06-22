#include "tool_file_create_binary.h"
#include "working_dir.h"
#include "format_util.h"
#include "console_writer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

// Base64 decoding table
static inline unsigned char b64_value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return 255;
}

static std::vector<unsigned char> Base64Decode(const std::string &input)
{
    std::vector<unsigned char> out;
    int val = 0, valb = -8;

    for (unsigned char c : input)
    {
        if (c == '=') break;
        unsigned char d = b64_value(c);
        if (d == 255) continue;

        val = (val << 6) + d;
        valb += 6;

        if (valb >= 0)
        {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

void FileCreateBinaryTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "path",
        "string",
        "Full path of the file to create.",
        true
        });

    params_acc.push_back({
        "content_base64",
        "string",
        "Base64-encoded binary content.",
        true
        });
}

std::string FileCreateBinaryTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("Binary file create tool.", console::TextOrigin::filesystem);

    std::string path = GetParam(params_values, "path");
    UnescapeSlashesInPath(path);

    std::string content_b64 = GetParam(params_values, "content_base64");

    if (path.empty())
    {
        console::write_line("Missing required parameter: path", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: path"}})";
    }

    if (content_b64.empty())
    {
        console::write_line("Missing required parameter: content_base64", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: content_base64"}})";
    }

    if (!wdir.IsInWorkDir(path))
    {
        console::write_line(fmt.Format("Permission denied: %?", path), console::TextOrigin::error);
        return fmt.Format("{\"error\":{\"type\":\"permission_denied\",\"message\":\"Path is outside working directory\",\"path\":\"%?\"}}", path);
    }

    console::write_line("Creating binary file: " + path, console::TextOrigin::filesystem);

    last_path = path;
    executed = false;
    existing_file_dump.clear();

    bool file_exists = std::filesystem::exists(path);

    // Save old content if overwriting
    if (file_exists)
    {
        std::ifstream in(path, std::ios::binary);
        existing_file_dump.assign(
            std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()
        );
    }

    // Decode base64
    last_content = Base64Decode(content_b64);

    // Write binary file
    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        console::write_line(fmt.Format("Failed to create file: %?", path), console::TextOrigin::error);
        return fmt.Format("{\"error\":{\"type\":\"runtime_error\",\"message\":\"Failed to create file\",\"path\":\"%?\"}}", path);
    }

    out.write(reinterpret_cast<const char *>(last_content.data()), last_content.size());
    out.close();

    executed = true;

    std::uintmax_t size = 0;
    try { size = std::filesystem::file_size(path); }
    catch (...) { size = last_content.size(); }

    return fmt.Format(
        "{"
        "\"status\":\"success\","
        "\"file\":{"
        "\"path\":\"%?\","
        "\"size_bytes\":%?,"
        "\"created\":true,"
        "\"overwritten\":%?"
        "},"
        "\"message\":\"File %?\""
        "}",
        path,
        size,
        file_exists ? "true" : "false",
        file_exists ? "overwritten" : "created"
    );
}

void FileCreateBinaryTool::Undo()
{
    if (!executed)
        return;

    console::write_line("Undo binary file create: " + last_path, console::TextOrigin::filesystem);

    if (!existing_file_dump.empty())
    {
        std::ofstream out(last_path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(existing_file_dump.data()), existing_file_dump.size());
    }
    else
    {
        if (std::filesystem::exists(last_path))
            std::filesystem::remove(last_path);
    }
}

void FileCreateBinaryTool::Redo()
{
    if (!executed)
        return;

    console::write_line("Redo binary file create: " + last_path, console::TextOrigin::filesystem);

    std::ofstream out(last_path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(last_content.data()), last_content.size());
}
