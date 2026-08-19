#ifndef HISTORY_MANAGER_INCLUDED_H
#define HISTORY_MANAGER_INCLUDED_H

#include "tool_base.h"
#include "console_writer.h"
#include <stack>
#include <memory>

class ToolParameter;

/// Tools calls history manager implementing Execute /Undo / Redo actions
class HistoryManager 
{
    public:
        std::string Execute(std::unique_ptr<ToolBase> cmd, std::vector<ToolParameter>& params_values) 
        {
            std::string response = cmd->Execute(params_values);

            if (cmd->CanBeReverted())
            {
                undo_stack.push(std::move(cmd));

                while (!redo_stack.empty())
                    redo_stack.pop();
            }

            return response;
        }

        void Undo() 
        {
            if (undo_stack.empty())
            {
                console::write_line("No operations to undo", console::TextOrigin::filesystem);
                return;
            } 

            auto cmd = std::move(undo_stack.top());
            undo_stack.pop();
            try 
            {
                cmd->Undo();
                redo_stack.push(std::move(cmd));
            } 
            catch (const std::exception &e) 
            {
                undo_stack.push(std::move(cmd));
                console::write_line(e.what(), console::TextOrigin::error);
            }
            catch (...)
            {
                undo_stack.push(std::move(cmd));
                console::write_line("Unknown error", console::TextOrigin::error);
            }
        }

        void Redo() 
        {
            if (redo_stack.empty())
            {
                console::write_line("No operations to redo.", console::TextOrigin::filesystem);
                return;
            }

            auto cmd = std::move(redo_stack.top());
            redo_stack.pop();
            try 
            {
                cmd->Redo();
                undo_stack.push(std::move(cmd));
            } 
            catch (const std::exception &e)
            {
                redo_stack.push(std::move(cmd));
                console::write_line(e.what(), console::TextOrigin::error);
            }
            catch (...)
            {
                redo_stack.push(std::move(cmd));
                console::write_line("Unknown error", console::TextOrigin::error);
            }
        }

    private:
        std::stack<std::unique_ptr<ToolBase>> undo_stack;
        std::stack<std::unique_ptr<ToolBase>> redo_stack;
};

#endif // HISTORY_MANAGER_INCLUDED_H