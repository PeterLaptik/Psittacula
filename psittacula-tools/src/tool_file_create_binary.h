#ifndef TOOL_FILE_CREATE_BINARY_INCLUDED_H
#define TOOL_FILE_CREATE_BINARY_INCLUDED_H

#include "tool_base.h"
#include <string>
#include <vector>

class FileCreateBinaryTool : public ToolBase
{
    public:
        FileCreateBinaryTool() = default;
        virtual ~FileCreateBinaryTool() = default;

        ToolBase *Clone() override { return new FileCreateBinaryTool(); }

        std::string Execute(std::vector<ToolParameter> &params_values) override;

        void Undo() override;
        void Redo() override;

        std::string GetToolName() const override { return "create_file_binary"; }
        std::string GetToolDescription() const override { return "Creates or overwrites a file with base64-encoded binary content."; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

    private:
        std::string last_path;
        std::vector<unsigned char> last_content;
        std::vector<unsigned char> existing_file_dump;
        bool executed = false;
};

#endif // TOOL_FILE_CREATE_BINARY_INCLUDED_H
