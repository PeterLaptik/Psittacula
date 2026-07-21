#ifndef FILE_CONTENT_MODIFY_INCLUDED_H
#define FILE_CONTENT_MODIFY_INCLUDED_H

#include "tool_base.h"
#include <string>
#include <vector>

class FileContentModifyTool : public ToolBase
{
    public:
        FileContentModifyTool() = default;
        ~FileContentModifyTool() override = default;

        ToolBase *Clone() override final { return new FileContentModifyTool(); }

        std::string Execute(std::vector<ToolParameter> &params_values) override;

        void Undo() override;
        void Redo() override;

        std::string GetToolName() const override { return "file_content_modify"; }
        std::string GetToolDescription() const override { return "Surgically modifies a file by replacing a specific target string with new content. The target string must be unique in the file."; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

    private:
        std::string last_path = "";
        std::string old_content = "";
        std::string new_content = "";
        bool executed = false;
};

#endif // FILE_CONTENT_MODIFY_INCLUDED_H
