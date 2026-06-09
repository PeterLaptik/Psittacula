#ifndef TOOL_FILE_COPY_INCLUDED_H
#define TOOL_FILE_COPY_INCLUDED_H

#include "tool_file.h"
#include <string>
#include <vector>

class FileCopyTool : public ToolFile
{
    public:
    FileCopyTool() = default;
    virtual ~FileCopyTool() = default;

    ToolBase *Clone() override { return new FileCopyTool(); }

    std::string Execute(std::vector<ToolParameter> &params_values) override;

    void Undo() override;
    void Redo() override;

    std::string GetToolName() const override { return "copy_file"; }
    std::string GetToolDescription() const override { return "Copies a file from one location to another."; }

    void GetParameters(std::vector<ToolParameter> &params_acc) override;

    private:
    std::string src_path;
    std::string dst_path;
    bool executed = false;
    bool dst_existed_before = false;
    std::string dst_backup; // backup of overwritten file
};

#endif // TOOL_FILE_COPY_INCLUDED_H
