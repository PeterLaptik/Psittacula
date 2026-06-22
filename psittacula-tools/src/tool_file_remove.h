#ifndef TOOL_FILE_REMOVE_INCLUDED_H
#define TOOL_FILE_REMOVE_INCLUDED_H

#include "tool_file.h"
#include <string>
#include <vector>

class FileRemoveTool : public ToolFile
{
    public:
        FileRemoveTool() = default;
        virtual ~FileRemoveTool() = default;

        ToolBase *Clone() override { return new FileRemoveTool(); }

        std::string Execute(std::vector<ToolParameter> &params_values) override;

        void Undo() override;
        void Redo() override;

        std::string GetToolName() const override { return "remove_file"; }
        std::string GetToolDescription() const override { return "Removes a file at the specified path."; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

    private:
        std::string last_path = "";
        std::string last_content = "";
        bool executed = false;
};

#endif // TOOL_FILE_REMOVE_INCLUDED_H