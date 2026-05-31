#include "tool_file_copy.h"
#include "working_dir.h"
#include "format_util.h"
#include "console_writer.h"

#include <filesystem>
#include <fstream>
#include <iostream>

void FileCopyTool::GetParameters(std::vector<ToolParameter> &params_acc)
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

std::string FileCopyTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("File copy tool.", console::TextOrigin::filesystem);

    std::string rel_from = GetParam(params_values, "from");
    std::string rel_to = GetParam(params_values, "to");

    if (rel_from.empty() || rel_to.empty())
    {
        console::write_line("Missing required parameters: from/to", console::TextOrigin::error);
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameters: from, to"}})";
    }

    src_path = std::filesystem::path(wdir.GetProjectDir() + rel_from).string();
    dst_path = std::filesystem::path(wdir.GetProjectDir() + rel_to).string();

    if (!wdir.IsInWorkDir(src_path) || !wdir.IsInWorkDir(dst_path))
    {
        console::write_line("Permission denied: path outside working directory", console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"permission_denied\",\"message\":\"One or both paths are outside working directory\",\"from\":\"%?\",\"to\":\"%?\"}}",
            rel_from, rel_to
        );
    }

    if (!std::filesystem::exists(src_path))
    {
        console::write_line(fmt.Format("Source file does not exist: %?", rel_from), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"not_found\",\"message\":\"Source file does not exist\",\"path\":\"%?\"}}",
            rel_from
        );
    }

    dst_existed_before = std::filesystem::exists(dst_path);
    dst_backup.clear();

    // If overwriting, save backup
    if (dst_existed_before)
    {
        std::ifstream in(dst_path, std::ios::binary);
        dst_backup.assign(
            std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()
        );
    }

    console::write_line(fmt.Format("Copying file: %? -> %?", src_path, dst_path), console::TextOrigin::filesystem);

    std::error_code ec;
    std::filesystem::copy_file(src_path, dst_path, std::filesystem::copy_options::overwrite_existing, ec);

    if (ec)
    {
        console::write_line(fmt.Format("Failed to copy file: %?", ec.message()), console::TextOrigin::error);
        return fmt.Format(
            "{\"error\":{\"type\":\"runtime_error\",\"message\":\"Failed to copy file: %?\",\"from\":\"%?\",\"to\":\"%?\"}}",
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
        "\"copied\":true,"
        "\"overwritten\":%?"
        "}"
        "}",
        rel_from, rel_to,
        dst_existed_before ? "true" : "false"
    );
}

void FileCopyTool::Undo()
{
    if (!executed)
        return;

    console::write_line("Undo file copy: removing " + dst_path, console::TextOrigin::filesystem);

    if (dst_existed_before)
    {
        // Restore previous content
        std::ofstream out(dst_path, std::ios::binary);
        out.write(dst_backup.data(), dst_backup.size());
    }
    else
    {
        if (std::filesystem::exists(dst_path))
            std::filesystem::remove(dst_path);
    }
}

void FileCopyTool::Redo()
{
    if (!executed)
        return;

    console::write_line("Redo file copy: " + src_path + " -> " + dst_path, console::TextOrigin::filesystem);

    std::error_code ec;
    std::filesystem::copy_file(src_path, dst_path, std::filesystem::copy_options::overwrite_existing, ec);
}
