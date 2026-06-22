#ifndef TOOL_FILE_CREATE_INCLUDED_H
#define TOOL_FILE_CREATE_INCLUDED_H

#include "tool_file.h"
#include <string>
#include <vector>

class FileCreateTool : public ToolFile
{
    public:
        FileCreateTool() = default;
        virtual ~FileCreateTool() = default;

        ToolBase *Clone() override { return new FileCreateTool(); }

        std::string Execute(std::vector<ToolParameter> &params_values) override;

        void Undo() override;
        void Redo() override;

        std::string GetToolName() const override { return "create_file"; }
        std::string GetToolDescription() const override { return "Creates a file with specified content."; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

    private:
        std::string last_path = "";
        std::string last_content = "";
        std::string existing_file_dump = "";
        bool executed = false;
};

#endif // TOOL_FILE_CREATE_INCLUDED_H
