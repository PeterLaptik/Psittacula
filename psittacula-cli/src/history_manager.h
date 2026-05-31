#ifndef HISTORY_MANAGER_INCLUDED_H
#define HISTORY_MANAGER_INCLUDED_H

#include "tool_base.h"
#include <stack>
#include <memory>

class ToolParameter;

class HistoryManager {
    public:
        std::string Execute(std::unique_ptr<ToolBase> cmd, std::vector<ToolParameter> &params_values) 
        {
            std::string response = cmd->Execute(params_values);
            undo_stack.push(std::move(cmd));

            while (!redo_stack.empty()) 
                redo_stack.pop();

            return response;
        }

        void Undo() 
        {
            if (undo_stack.empty()) 
                return;
            auto cmd = std::move(undo_stack.top());
            undo_stack.pop();
            cmd->Undo();
            redo_stack.push(std::move(cmd));
        }

        void Redo() 
        {
            if (redo_stack.empty()) 
                return;
            auto cmd = std::move(redo_stack.top());
            redo_stack.pop();
            cmd->Redo();
            undo_stack.push(std::move(cmd));
        }

    private:
        std::stack<std::unique_ptr<ToolBase>> undo_stack;
        std::stack<std::unique_ptr<ToolBase>> redo_stack;
};


#endif // !HISTORY_MANAGER_INCLUDED_H