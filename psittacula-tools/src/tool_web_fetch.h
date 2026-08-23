#ifndef TOOL_WEB_FETCH_INCLUDED_H
#define TOOL_WEB_FETCH_INCLUDED_H

#include "tool_base.h"
#include <string>
#include <vector>

class WebFetchTool : public ToolBase
{
    public:
        WebFetchTool() = default;
        ~WebFetchTool() override = default;

        ToolBase *Clone() override final { return new WebFetchTool(); }

        std::string Execute(std::vector<ToolParameter> &params_values) override;

        void Undo() override {}
        void Redo() override {}

        std::string GetToolName() const override { return "web_fetch"; }
        std::string GetToolDescription() const override { return "Fetches a URL over HTTP(S) and returns the response body as text or base64-encoded binary. Supports custom headers, POST bodies, timeouts, and response size limits."; }

        void GetParameters(std::vector<ToolParameter> &params_acc) override;

        bool CanBeReverted() override { return false; }
};

#endif // TOOL_WEB_FETCH_INCLUDED_H
