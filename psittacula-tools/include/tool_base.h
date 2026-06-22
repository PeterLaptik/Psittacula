#ifndef TOOL_BASE_INCLUDED_H
#define TOOL_BASE_INCLUDED_H

#include <string>
#include <vector>

struct ToolCall
{
    std::string id;
    std::string name;
    std::vector<std::pair<std::string, std::string>> arguments; // Parsed arguments from JSON content
    std::string content; // JSON content
};

struct ToolResponse
{
    std::string role = "tool";
    std::string name;
    std::string output_content;
    std::string input_content;
    std::string id;
};

struct ToolParameter
{
    std::string name;
    std::string type;
    std::string description;
    bool is_required;
    std::string default;
    std::string value; // it is always string, it is used as a tool parameter after a casting
};

class ToolBase
{
    public:
        ToolBase() = default;

        virtual ~ToolBase() = default;

        virtual ToolBase* Clone() = 0;

        // Returns tool response as a JSON string
        virtual std::string Execute(std::vector<ToolParameter> &params_values) = 0;

        virtual void Undo() = 0;

        virtual void Redo() = 0;

        virtual std::string GetToolName() const = 0;

        virtual std::string GetToolDescription() const = 0;

        virtual void GetParameters(std::vector<ToolParameter> &params_acc) = 0;

        /// Defines: should be added to undo / redo history
        virtual bool CanBeReverted() { return true; }

    protected:
        std::string GetParam(const std::vector<ToolParameter> &params_acc, const std::string &param_name, std::string default_value = "");

        bool GetParamBool(const std::vector<ToolParameter> &params_acc, const std::string &param_name, bool default_value);

        std::string GetEscapedJSONString(const std::string &str) const;

        void UnescapeSlashesInPath(std::string &value) const;
};

#endif // TOOL_BASE_INCLUDED_H