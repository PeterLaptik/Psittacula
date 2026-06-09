#ifndef TOOL_DIR_DELETE_INCLUDED_H
#define TOOL_DIR_DELETE_INCLUDED_H

#include "tool_file.h"
#include <string>
#include <vector>

class DirDeleteTool : public ToolFile
{
    public:
        DirDeleteTool() = default;
        virtual ~DirDeleteTool() = default;

        ToolBase *Clone() override { return new DirDeleteTool(); }

        std::string Execute(std::vector<ToolParameter> &params_values) override;

        void Undo() override;
        void Redo() override;

        std::string GetToolName() const override { return "delete_directory"; }
        std::string GetToolDescription() const override { return "Deletes a directory (empty or recursive). Dangerous!"; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

    private:
        std::string last_path;
        bool executed = false;
        bool recursive = false;
        bool existed_before = false;
};

#endif // TOOL_DIR_DELETE_INCLUDED_H

