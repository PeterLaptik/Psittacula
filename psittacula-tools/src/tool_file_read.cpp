#include "tool_file_read.h"
#include "working_dir.h"
#include "format_util.h"
#include "console_writer.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <vector>

// Simple base64 encoder
static const std::string BASE64_CHARS =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

static std::string Base64Encode(const std::vector<unsigned char> &data)
{
    std::string out;
    int val = 0, valb = -6;

    for (unsigned char c : data)
    {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0)
        {
            out.push_back(BASE64_CHARS[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6)
        out.push_back(BASE64_CHARS[((val << 8) >> (valb + 8)) & 0x3F]);

    while (out.size() % 4)
        out.push_back('=');

    return out;
}

void FileReadTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "path",
        "string",
        "Full path of the file to read.",
        true
        });

    params_acc.push_back({
        "binary",
        "boolean",
        "If true, return file content as base64-encoded binary.",
        false
        });
}

std::string FileReadTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("File read tool.", console::TextOrigin::filesystem);

    std::string path = GetParam(params_values, "path");

    bool binary_mode = GetParamBool(params_values, "binary", false);

    console::write_line(fmt.Format("Reading file: %?", path), console::TextOrigin::filesystem);

    if (path.empty())
    {
        console::write_line("Missing required parameter: path", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: path"}})";
    }

    if (!wdir.IsInWorkDir(path))
    {
        console::write_line(fmt.Format("Permission_denied: path is outside working directory: %?", path), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"permission_denied\",\"message\":\"Path is outside working directory\",\"path\":\"%?\"}}",
            path
        );
    }

    if (!std::filesystem::exists(path))
    {
        console::write_line(fmt.Format("File does not exist: %?", path), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"not_found\",\"message\":\"File does not exist\",\"path\":\"%?\"}}",
            path
        );
    }

    std::uintmax_t size = 0;
    try 
    { 
        size = std::filesystem::file_size(path); 
    }
    catch (...) { }

    if (binary_mode)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            console::write_line(fmt.Format("Failed to open file: %?", path), console::TextOrigin::error);
            return fmt.Format(
                "{\"error\":{\"type\":\"runtime_error\",\"message\":\"Failed to open file\",\"path\":\"%?\"}}",
                path
            );
        }

        std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>());

        std::string b64 = Base64Encode(buffer);

        std::string result_template =
            "{"
            "  \"status\": \"success\","
            "  \"file\": {"
            "    \"path\": \"%?\","
            "    \"size_bytes\": %?,"
            "    \"binary\": true,"
            "    \"content_base64\": \"%?\""
            "  }"
            "}";

        return fmt.Format(result_template, path, size, b64);
    }

    std::ifstream in(path);
    if (!in)
    {
        return fmt.Format(
            "{\"error\":{\"type\":\"runtime_error\",\"message\":\"Failed to open file\",\"path\":\"%?\"}}",
            path
        );
    }

    std::stringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();

    std::string result_template =
        "{"
        "  \"status\": \"success\","
        "  \"file\": {"
        "    \"path\": \"%?\","
        "    \"size_bytes\": %?,"
        "    \"binary\": false,"
        "    \"content\": %?"
        "  },"
        "  \"message\": \"File read successfully\""
        "}";

    std::string file_content = GetEscapedJSONString(content);
    return fmt.Format(result_template, path, size, file_content);
}