#ifndef TOOL_FACTORY_INCLUDED_H
#define TOOL_FACTORY_INCLUDED_H

#include <vector>

class ToolBase;

void get_all_tools(std::vector<ToolBase *> &acc);

#endif // TOOL_FACTORY_INCLUDED_H