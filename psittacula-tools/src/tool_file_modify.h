#ifndef TOOL_FILE_MODIFY_INCLUDED_H
#define TOOL_FILE_MODIFY_INCLUDED_H

#include "tool_file.h"
#include <string>
#include <vector>

class FileModifyTool : public ToolFile
{
    public:
        FileModifyTool() = default;
        virtual ~FileModifyTool() = default;

        ToolBase *Clone() override { return new FileModifyTool(); }

        std::string Execute(std::vector<ToolParameter> &params_values) override;

        void Undo() override;
        void Redo() override;

        std::string GetToolName() const override { return "modify_file"; }
        std::string GetToolDescription() const override { return "Modifies a file by replacing content at a specific position or replacing entire content."; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

    private:
        std::string last_path = "";
        std::string old_content = "";
        std::string new_content = "";
        size_t position = 0; // Position to insert/replace at (0 = beginning, npos = append)
        bool executed = false;
        bool replace_all = false; // If true, replaces entire content; if false, inserts at position
};

#endif // TOOL_FILE_MODIFY_INCLUDED_H