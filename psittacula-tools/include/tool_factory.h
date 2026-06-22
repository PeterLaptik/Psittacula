#ifndef TOOL_FACTORY_INCLUDED_H
#define TOOL_FACTORY_INCLUDED_H

#include <vector>
#include <memory>

class ToolBase;

void get_all_tools(std::vector<std::unique_ptr<ToolBase>> &acc);

#endif // TOOL_FACTORY_INCLUDED_H