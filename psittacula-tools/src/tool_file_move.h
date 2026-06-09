#ifndef TOOL_FILE_MOVE_INCLUDED_H
#define TOOL_FILE_MOVE_INCLUDED_H

#include "tool_file.h"
#include <string>
#include <vector>

class FileMoveTool : public ToolFile
{
    public:
        FileMoveTool() = default;
        virtual ~FileMoveTool() = default;

        ToolBase *Clone() override { return new FileMoveTool(*this); }

        std::string Execute(std::vector<ToolParameter> &params_values) override;

        void Undo() override;
        void Redo() override;

        std::string GetToolName() const override { return "move_file"; }
        std::string GetToolDescription() const override { return "Moves or renames a file."; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

    private:
        std::string old_path;
        std::string new_path;
        bool executed = false;
};

#endif // TOOL_FILE_MOVE_INCLUDED_H
