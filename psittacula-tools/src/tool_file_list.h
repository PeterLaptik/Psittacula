#ifndef TOOL_FILE_LIST_INCLUDED_H
#define TOOL_FILE_LIST_INCLUDED_H

#include "tool_file.h"
#include <string>
#include <vector>

class FileListTool : public ToolFile
{
    public:
        FileListTool() = default;
        virtual ~FileListTool() = default;

        ToolBase *Clone() override { return new FileListTool(); }

        std::string Execute(std::vector<ToolParameter> &params_values) override;

        void Undo() override {}
        void Redo() override {}

        std::string GetToolName() const override { return "list_files"; }
        std::string GetToolDescription() const override { return "Lists files and directories inside a given path."; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

    private:
        std::string BuildListingJSON(const std::string &path,
            bool recursive,
            bool include_hidden);
};

#endif // TOOL_FILE_LIST_INCLUDED_H
