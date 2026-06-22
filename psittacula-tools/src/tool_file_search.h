#ifndef TOOL_FILE_SEARCH_INCLUDED_H
#define TOOL_FILE_SEARCH_INCLUDED_H

#include "tool_file.h"
#include <string>
#include <vector>

class FileSearchTool : public ToolFile
{
    public:
        FileSearchTool() = default;
        ~FileSearchTool() override = default;

        ToolBase *Clone() override { return new FileSearchTool(); }

        std::string Execute(std::vector<ToolParameter> &params_values) override;
        void Undo() override { };
        void Redo() override { };

        std::string GetToolName() const override { return "file_search"; }
        std::string GetToolDescription() const override { return "Searches files within a directory for lines matching a query or regex pattern.";; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

        bool CanBeReverted() override { return false; }

    private:
        struct Match
        {
            std::string file;
            int line;
            std::string snippet;
        };

        std::vector<Match> SearchDirectory(
            const std::string &root,
            const std::string &query,
            bool case_sensitive,
            bool use_regex);

        std::vector<Match> SearchFile(
            const std::string &path,
            const std::string &query,
            bool case_sensitive,
            bool use_regex);
};

#endif // TOOL_FILE_SEARCH_INCLUDED_H
