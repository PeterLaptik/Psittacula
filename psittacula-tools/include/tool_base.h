#ifndef TOOL_BASE_INCLUDED_H
#define TOOL_BASE_INCLUDED_H

#include <string>
#include <vector>

struct ToolCall
{
    std::string id;
    std::string name;
    std::vector<std::pair<std::string, std::string>> arguments;
    std::string content;
};

struct ToolResponse
{
    std::string role = "tool";
    std::string name;
    std::string output_content;
    std::string input_content;
};

struct ToolParameter
{
    std::string name;
    std::string type;
    std::string description;
    bool is_required;

    std::string value = ""; // always string, it is used to evoke a tool
};

class ToolBase
{
    public:
        ToolBase() = default;

        virtual ~ToolBase() = default;

        virtual ToolBase* Clone() = 0;

        // Retutns tool response JSON string
        virtual std::string Execute(std::vector<ToolParameter> &params_values) = 0;

        virtual void Undo() = 0;

        virtual void Redo() = 0;

        virtual std::string GetToolName() const = 0;

        virtual std::string GetToolDescription() const = 0;

        virtual void GetParameters(std::vector<ToolParameter> &params_acc) = 0;

    protected:
        std::string GetParam(const std::vector<ToolParameter> &params_acc, const std::string &param_name, std::string default_value = "");

        bool GetParamBool(const std::vector<ToolParameter> &params_acc, const std::string &param_name, bool default_value);

        // Util method to escape symbols for string putting into a JSON
        std::string FormatJSONString(const std::string &str);

        void CleanFilePathFromTrailingDots(std::string path);
};

#endif // TOOL_BASE_INCLUDED_H