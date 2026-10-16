#include "tool_file_read.h"
#include "working_dir.h"
#include "format_util.h"
#include "console_writer.h"
#include "utf8_util.h"
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

namespace {
    constexpr std::uintmax_t kMaxBinaryBytes = 1024 * 1024;        // 1 MiB
    constexpr size_t kMaxTextBytes = 256 * 1024;                   // 256 KiB

    // Clips to max_bytes without splitting a UTF-8 sequence
    std::string ClipUtf8(const std::string &s, size_t max_bytes)
    {
        if (s.size() <= max_bytes)
            return s;

        size_t end = max_bytes;
        while (end > 0 && (static_cast<unsigned char>(s[end]) & 0xC0) == 0x80)
            --end;

        return s.substr(0, end);
    }
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

    console::write_line("File read tool.", console::TextOrigin::tools);

    std::string path = GetParam(params_values, "path");

    bool binary_mode = GetParamBool(params_values, "binary", false);

    console::write_line(fmt.Format("Path: %?", path), console::TextOrigin::tools);

    if (path.empty())
    {
        console::write_line("Missing required parameter: path", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: path"}})";
    }

    if (!wdir.IsInWorkDir(path))
    {
        console::write_line(fmt.Format("Permission_denied: path is outside working directory:\n path: %?\n working directory: %?", path, wdir.GetProjectDir()), console::TextOrigin::error);
        return fmt.Format("{\"error\":{\"type\":\"permission_denied\",\"message\":\"Path is outside working directory (%?)\",\"path\":\"%?\"}}", wdir.GetProjectDir(), path);
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

    // Early cap check: refuse / report before any memory is allocated
    if (binary_mode && size > kMaxBinaryBytes)
    {
        console::write_line(fmt.Format("File too large for a binary read: %? bytes (limit: %?).", size, kMaxBinaryBytes), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"too_large\",\"message\":\"File exceeds the binary read limit\",\"size_bytes\":%?,\"limit_bytes\":%?,\"path\":\"%?\"}}",
            size,
            (long long)kMaxBinaryBytes,
            path
        );
    }

    if (!binary_mode && size > kMaxTextBytes)
    {
        console::write_line(fmt.Format("File is large and will be truncated: %? bytes (kept: first %?).", size, kMaxTextBytes), console::TextOrigin::tools);
        // not an error: the read below returns the first kMaxTextBytes with
        // truncated=true, a partial read is still useful for text
    }

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

        // Capped read
        std::vector<unsigned char> buffer;
        buffer.reserve(size < kMaxBinaryBytes ? static_cast<size_t>(size) : static_cast<size_t>(kMaxBinaryBytes));

        char chunk[65536];
        bool binary_over_cap = false;

        while (in.read(chunk, sizeof(chunk)) || in.gcount() > 0)
        {
            const std::streamsize got = in.gcount();

            if (buffer.size() + static_cast<size_t>(got) > kMaxBinaryBytes)
            {
                binary_over_cap = true; // file_size lied: enforce now
                break;
            }

            buffer.insert(buffer.end(), chunk, chunk + got);
        }

        if (binary_over_cap)
        {
            console::write_line(fmt.Format("File too large for a binary read: over %? bytes.", kMaxBinaryBytes), console::TextOrigin::error);
            return fmt.Format(
                "{\"error\":{\"type\":\"too_large\",\"message\":\"File exceeds the binary read limit\",\"limit_bytes\":%?,\"path\":\"%?\"}}",
                (long long)kMaxBinaryBytes,
                path
            );
        }

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

    // Capped read
    std::string content;
    content.reserve(size < kMaxTextBytes ? static_cast<size_t>(size) : kMaxTextBytes);

    char chunk[65536];
    bool truncated = false;

    while (in.read(chunk, sizeof(chunk)) || in.gcount() > 0)
    {
        const std::streamsize got = in.gcount();
        const size_t space = kMaxTextBytes - content.size();

        if (space == 0)
        {
            truncated = true; // there was more to read
            break;
        }

        if (static_cast<size_t>(got) > space)
        {
            content.append(chunk, space);
            truncated = true;
            break;
        }

        content.append(chunk, static_cast<size_t>(got));
    }

    content = ClipUtf8(content, kMaxTextBytes);

    std::string result_template =
        "{"
        "  \"status\": \"success\","
        "  \"file\": {"
        "    \"path\": \"%?\","
        "    \"size_bytes\": %?,"
        "    \"binary\": false,"
        "    \"content\": %?"
        "  },"
        "  \"message\": \"File read successfully%?\""
        "}";

    std::string file_content = GetEscapedJSONString(utf8::AnsiToUtf8(content));
    return fmt.Format(result_template, path, size, file_content,
        truncated ? " (truncated to the first 262144 bytes; split the file or read specific ranges if more is needed)" : "");
}
