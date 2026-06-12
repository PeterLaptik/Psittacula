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
#include <memory>

void get_all_tools(std::vector<std::unique_ptr<ToolBase>> &acc)
{
    acc.push_back(std::make_unique<DirCreateTool>());
    acc.push_back(std::make_unique<DirDeleteTool>());
    acc.push_back(std::make_unique<FileListTool>());
    acc.push_back(std::make_unique<FileCreateTool>());
    acc.push_back(std::make_unique<FileRemoveTool>());
    acc.push_back(std::make_unique<FileCopyTool>());
    acc.push_back(std::make_unique<FileMoveTool>());
    acc.push_back(std::make_unique<FileModifyTool>());
    acc.push_back(std::make_unique<FileReadTool>());
    acc.push_back(std::make_unique<FileCreateBinaryTool>());
    acc.push_back(std::make_unique<FileSearchTool>());
}
