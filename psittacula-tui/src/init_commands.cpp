#include "init_commands.h"
#include "command_change_model.h"
#include "command_tools.h"
#include "command_reasoning.h"
#include "command_undo.h"
#include "command_redo.h"
#include "command_dump.h"
#include "command_dump_text.h"
#include "command_change_agent.h"
#include "command_change_project_dir.h"
#include "command_clean_context.h"
#include "command_restore.h"
#include "command_compress.h"

#include <memory>

void init_commands(ChatCommandDispatcher *dsp, tui::App *app)
{
    dsp->RegisterCommand("model", std::make_unique<CommandChangeModel>(app));
    dsp->RegisterCommand("tools", std::make_unique<CommandTools>(app));
    dsp->RegisterCommand("reasoning", std::make_unique<CommandReasoning>(app));
    dsp->RegisterCommand("undo", std::make_unique<CommandUndo>(app));
    dsp->RegisterCommand("redo", std::make_unique<CommandRedo>(app));
    dsp->RegisterCommand("dump", std::make_unique<CommandDump>(app));
    dsp->RegisterCommand("dump_text", std::make_unique<CommandDumpText>(app));
    dsp->RegisterCommand("rules", std::make_unique<CommandChangeRules>(app));
    dsp->RegisterCommand("project", std::make_unique<CommandChangeProjectDir>(app));
    dsp->RegisterCommand("clear", std::make_unique<CommandCleanContext>(app));
    dsp->RegisterCommand("restore", std::make_unique<CommandRestore>(app));
    dsp->RegisterCommand("compress", std::make_unique<CommandCompress>(app));
}