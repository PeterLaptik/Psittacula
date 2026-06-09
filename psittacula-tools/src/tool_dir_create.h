#ifndef TOOL_DIR_CREATE_INCLUDED_H
#define TOOL_DIR_CREATE_INCLUDED_H

#include "tool_file.h"
#include <string>
#include <vector>

class DirCreateTool : public ToolFile
{
    public:
        DirCreateTool() = default;
        virtual ~DirCreateTool() = default;

        ToolBase *Clone() override { return new DirCreateTool(); }

        std::string Execute(std::vector<ToolParameter> &params_values) override;
        void Undo() override;
        void Redo() override;

        std::string GetToolName() const override { return "create_dir"; }
        std::string GetToolDescription() const override { return "Creates a directory."; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

    private:
        std::string last_path = "";
        bool executed = false;
        bool dir_existed_before = false;
};

#endif // TOOL_DIR_CREATE_INCLUDED_H