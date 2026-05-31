#include "tool_factory.h"
#include "tool_file_create.h"
#include "tool_file_copy.h"
#include "tool_dir_create.h"
#include "tool_dir_delete.h"
#include "tool_file_remove.h"
#include "tool_file_modify.h"
#include "tool_file_read.h"
#include "tool_file_list.h"
#include "tool_file_move.h"
#include "tool_file_create_binary.h"
#include "tool_file_search.h"

void get_all_tools(std::vector<ToolBase*> &acc)
{
    acc.push_back(new DirCreateTool());
    acc.push_back(new DirDeleteTool());
    acc.push_back(new FileListTool());
    acc.push_back(new FileCreateTool());
    acc.push_back(new FileRemoveTool());
    acc.push_back(new FileCopyTool());
    acc.push_back(new FileMoveTool());
    acc.push_back(new FileModifyTool());
    acc.push_back(new FileReadTool());
    acc.push_back(new FileCreateBinaryTool());
    acc.push_back(new FileSearchTool());
}