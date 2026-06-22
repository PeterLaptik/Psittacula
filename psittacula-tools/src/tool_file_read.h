#ifndef TOOL_FILE_READ_INCLUDED_H
#define TOOL_FILE_READ_INCLUDED_H

#include "tool_file.h"
#include <string>
#include <vector>

class FileReadTool : public ToolFile
{
    public:
        FileReadTool() = default;
        virtual ~FileReadTool() = default;

        ToolBase *Clone() override { return new FileReadTool(); }

        std::string Execute(std::vector<ToolParameter> &params_values) override;

        void Undo() override {}
        void Redo() override {}

        std::string GetToolName() const override { return "read_file"; }
        std::string GetToolDescription() const override { return "Reads the content of a file (text or binary)."; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

        bool CanBeReverted() override { return false; }
};

#endif // TOOL_FILE_READ_INCLUDED_H
