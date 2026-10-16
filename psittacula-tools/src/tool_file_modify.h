#ifndef TOOL_FILE_MODIFY_INCLUDED_H
#define TOOL_FILE_MODIFY_INCLUDED_H

#include "tool_file.h"
#include <string>
#include <vector>

class FileModifyTool : public ToolFile
{
public:
    FileModifyTool() = default;
    ~FileModifyTool() override = default;

    ToolBase *Clone() override final { return new FileModifyTool(); }

    std::string Execute(std::vector<ToolParameter> &params_values) override;

    void Undo() override;
    void Redo() override;

    std::string GetToolName() const override { return "modify_file"; }
    std::string GetToolDescription() const override {
        return "Modifies a file using explicit modes: overwrite, insert, append, replace_range.";
    }

    void GetParameters(std::vector<ToolParameter> &params_acc) override;

private:
    std::string last_path;
    std::string old_content;
    std::string new_content;

    std::string mode;
    size_t position = 0;
    size_t range_start = 0;
    size_t range_end = 0;

    bool executed = false;
};

#endif // TOOL_FILE_MODIFY_INCLUDED_H